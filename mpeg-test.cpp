#include "msvd.hpp"
#include "mpeg.hpp"
#include "mvdu.hpp"


namespace utils { inline namespace async_ops {

template<typename R, typename F>
struct generic_async_op {
  F f;

  template<typename F2>
  auto operator +=(F2 func) -> typename std::enable_if<is_callable<F2(R)>::value>::type {
    f(std::move(func));
  }
};

template<typename R, typename F>
struct is_async_op<generic_async_op<R, F>> : std::true_type {};

template<typename R, typename F>
struct async_result<generic_async_op<R,F>> {
  using type = R;
};

template<typename R, typename F>
generic_async_op<R, F> make_generic_async_op(F func) {
  return {std::move(func)};
}

}}

struct async_open_wrap {
  asio::io_service& io;

  template<typename F>
  void operator()(F func) { 
    msvd::async_open(io, utils::expected_to_asio(std::move(func)));
  };
};

auto make_async_open_op(asio::io_service& io) -> utils::generic_async_op<utils::expected<msvd::device, std::error_code>, async_open_wrap> {
  return utils::make_generic_async_op<utils::expected<msvd::device, std::error_code>>(async_open_wrap{io});
}


template<typename UD>
struct async_allocate_wrap {
  mvdu::handle& video;
  UD user_data;

  using result_type = utils::expected<std::shared_ptr<mvdu::buffer<UD>>, std::error_code>;

  template<typename F>
  void operator()(F func) {
    async_allocate(video, std::move(user_data), utils::expected_to_asio(std::move(func)));
  }
};

template<typename UD>
utils::generic_async_op<typename async_allocate_wrap<UD>::result_type, async_allocate_wrap<UD>> async_allocate(mvdu::handle& video, UD ud) {
  return {{video, std::move(ud)}};
}

struct mpeg_headers {
  mpeg::sequence_header_t sh;
  mpeg::picture_header_t ph;
  bool mpeg2;
  mpeg::picture_coding_extension_t pcx;
};

template<typename Buffer>
struct mpeg_context {
  mpeg_headers headers;
  std::array<Buffer,10> refs;
};

template<typename Buffer>
struct async_decode_wrap {
  msvd::device device;
  
  mpeg_context<Buffer> ctx;
  Buffer curpic;

  asio::const_buffers_1 buffer;

  template<typename F>
  void operator()(F func) {
    auto b = buffer.begin();
    msvd::async_decode(
      std::move(device),
      ctx.headers.sh,
      ctx.headers.ph,
      ctx.headers.mpeg2 ? &ctx.headers.pcx : nullptr,
      {1920, 1088, 0, mvdu::buffer_chroma_offset},
      phys_addr(curpic),
      ctx.refs[0] ? phys_addr(ctx.refs[0]) : 0,
      ctx.refs[1] ? phys_addr(ctx.refs[1]) : 0,
      utils::make_range(&*b, &*b+1),
      utils::expected_to_asio(std::move(func)));
  }
};

template<typename Buffer>
utils::generic_async_op<utils::expected<msvd::device, std::error_code>, async_decode_wrap<Buffer>> async_decode(
  msvd::device device,
  mpeg_context<Buffer>& ctx,
  Buffer curpic,
  asio::const_buffers_1 buffer)
{
  return {{std::move(device), ctx, std::move(curpic), buffer}};
}

using namespace H264;

template<typename P>
RBSP<P> make_parser(P p) {
  return RBSP<P>(std::move(p));
}

template<typename A>
MemStream read_access_unit(Stream<A>& s) {
  using namespace mpeg;

  auto next_picture_start_code = [&](std::size_t p) {
    auto scode = {0, 0, 1, 0};
    auto e = std::search(s.gptr_ + p, s.egptr_, std::begin(scode), std::end(scode));
    for(std::size_t n = 4096;s && e == s.egptr_; n += std::min(n, std::size_t(32*1024u))) {
      s.make_avail(n);
      e = std::search(s.gptr_ + p, s.egptr_, std::begin(scode), std::end(scode));
    }

    return e - s.gptr_;
  };

  s.make_avail(4);

  std::size_t p = 0;
  if(s.gptr_[p+3] == sequence_header_code || s.gptr_[p+3] == group_start_code) {
    p = next_picture_start_code(p);
    s.make_avail(4);
  }

  assert(s.gptr_[p+3] == picture_start_code);

  p = next_picture_start_code(p+4);

  return MemStream{s.gptr_, s.gptr_+p};
}

template<typename P>
void skip_to_next_start_code(RBSP<P>& a) {
  search(a.a_, {0,0,1});
  H264::advance(a.a_, 3);
  u(a, a.available_bits());   
}

void parse_headers(MemStream& au, mpeg_headers& headers) {
  using namespace mpeg;  
  auto parser = make_parser(std::ref(au));   
 
  auto process_extensions_and_user_data = [&]() {
    for(;;) {
      skip_to_next_start_code(parser);
      auto hc = u(parser, 8);

      if(hc == extension_start_code) {
        if(u(parser, 4) == quant_matrix_extension_id) {
          auto qmx = quant_matrix_extension(parser);
          if(qmx.load_intra_quantiser_matrix) headers.sh.intra_quantiser_matrix = qmx.intra_quantiser_matrix;
          if(qmx.load_non_intra_quantiser_matrix) headers.sh.non_intra_quantiser_matrix = qmx.non_intra_quantiser_matrix;
         } 
      } 
      if(hc != extension_start_code && hc != user_data_start_code) return hc;    
    }
  };
 
  u(parser, 24);
    
  auto hc = u(parser, 8);
 
  if(hc == mpeg::sequence_header_code) {
    headers.sh = sequence_header(parser);
    if(!headers.sh.load_intra_quantiser_matrix) headers.sh.intra_quantiser_matrix = default_intra_quantiser_matrix;
    if(!headers.sh.load_non_intra_quantiser_matrix) headers.sh.non_intra_quantiser_matrix = default_non_intra_quantiser_matrix;   

    hc = process_extensions_and_user_data(); 
  }

  if(hc == group_start_code) {
    group_of_pictures_header(parser);
    hc = process_extensions_and_user_data();
  }

  assert(hc == picture_start_code);
  headers.ph = picture_header(parser);
  skip_to_next_start_code(parser);

  hc = u(parser, 8);

  if(hc == extension_start_code) {
    if(u(parser, 4) == picture_coding_extension_id) {
      headers.pcx = picture_coding_extension(parser);
      headers.mpeg2 = true;
    }
    
    hc = process_extensions_and_user_data();
  }
    
  assert(hc == slice_start_code_begin);

  au.first -= (parser.available_bits() + 7) / 8;
  au.first -= 4;
}

auto async_decode_au = [](mpeg_context<std::shared_ptr<mvdu::buffer<int>>>& ctx, mvdu::handle& video, msvd::device device, MemStream au) {
  auto decoder = utils::move_on_copy(std::move(device));

  parse_headers(au, ctx.headers);

  std::cout << "async_decode_au" << std::endl;

  return async_allocate(video, 0)
    >> 
    [=,&ctx](std::shared_ptr<mvdu::buffer<int>> buffer) mutable {
      memset(luma_buffer(buffer), 0x80, mvdu::buffer_size);
      auto decode = 
        async_decode(std::move(unwrap(decoder)), ctx, buffer, asio::const_buffers_1(au.first, au.second - au.first))
        >>
        [=](msvd::device device) {
          return std::make_tuple(std::move(device), buffer);
        };

      if(ctx.headers.ph.picture_coding_type != mpeg::picture_coding::B) {
        std::copy_backward(ctx.refs.begin(), ctx.refs.end()-1, ctx.refs.end());
        ctx.refs[0] = buffer;
      }

      return decode;
    };
};

template<typename A>
void async_decode_stream(msvd::device d, mpeg_context<std::shared_ptr<mvdu::buffer<int>>>& ctx, mvdu::handle& video, Stream<A>& s) {
  if(!s) return;

  auto au = read_access_unit(s);
  async_decode_au(ctx, video, std::move(d), au)
  >> [&, au] (msvd::device device, std::shared_ptr<mvdu::buffer<int>> buf) {
    s.advance(au.second - au.first);
    async_render(video, std::move(buf), [](std::error_code const&, std::shared_ptr<mvdu::buffer<int>>){});
      
    //char c;
    //std::cin >> c;

    return device;
  }
  += 
  [&](std::error_code const& ec, msvd::device d) {
    std::cout << ec << std::endl;
    async_decode_stream(std::move(d), ctx, video, s);
  };
}

int main(int argc, char* argv[]) {
  using namespace msvd;

  asio::io_service io;

  auto fd = ::open(argv[1], O_RDONLY);

  auto stream = make_stream([=](std::uint8_t* buffer, std::size_t n) {
    auto r = ::read(fd, buffer, n);  
    if(r < 0) throw std::system_error(errno, std::system_category());
    return r;
  });

  auto video = mvdu::open(io); 
  set_params(video, mvdu::video_mode::hd, {0,0,1920,1080}, {0,0,1920, 1080});
  
  mpeg_context<std::shared_ptr<mvdu::buffer<int>>> ctx;

  make_async_open_op(io) += [&] (std::error_code const& ec, device d) mutable {
    std::cout << ec << std::endl;

    async_decode_stream(std::move(d), ctx, video, stream);
  };

  io.run();
}

