#ifndef __transport_stream_hpp_aac2597c_3f6a_406f_9316_8357a47b03f2__
#define __transport_stream_hpp_aac2597c_3f6a_406f_9316_8357a47b03f2__

#include "utils.hpp"
#include "bitstream.hpp"

namespace media {
namespace mpeg {
namespace ts {

const std::uint8_t sync_byte = 0x47;
const std::size_t packet_length = 188;

enum class errc {
  out_of_sync = 1,
  framing_error,
  invalid_adaptation_field_length,
  invalid_adaptation_field_control_code
};

inline
std::error_category const& error_category() noexcept {
 	static struct : public std::error_category {
		const char* name() const noexcept { return "mpeg::transport_stream"; }
 
    virtual std::string message(int ev) const {
			switch(static_cast<errc>(ev)) {
			case errc::out_of_sync: return "mpeg::ts out of sync";
      case errc::framing_error: return "mpeg::ts framing error";
      case errc::invalid_adaptation_field_length: return "mpeg::ts invalid adaptation field length";
      case errc::invalid_adaptation_field_control_code: return "mpeg::ts::errc::invalid_adaptation_field_control_code";
      default: return "unknown error";
			};
		}
	} cat;
  return cat;
}

inline
std::error_code make_error_code(errc e) { return {static_cast<int>(e), error_category()}; }

struct packet_tag {};

template<typename BS>
using packet = utils::tagged_byte_sequence<packet_tag, BS>;

struct header {
  unsigned sync_byte                    : 8;
  unsigned transport_error_indicator    : 1;
  unsigned payload_unit_start_indicator : 1;
  unsigned transport_priority           : 1;
  unsigned pid                          : 13;
  unsigned transport_scrambling_control : 2;
  unsigned adaptation_field_control     : 2;
  unsigned continuity_counter           : 4;
};

template<typename Stream>
auto read_packet(Stream&& s, asio::streambuf& buffer, std::size_t pos = 0) -> decltype(buffer.data()) {
  for(;;) {
    auto d = buffer.data();
    if(buffer_size(d) < packet_length + pos) {
      auto n = s(buffer.prepare(packet_length*1000));
      if(n == 0) return subsequence(buffer.data(), 0, 0);
      buffer.commit(n);
    }
    else
      return subsequence(buffer.data(), pos, packet_length);
  }
}

template<typename BS>
header parse_header(packet<BS> const& s) {
  auto parser = bitstream::make_bit_parser(bitstream::make_bit_range(utils::make_range(begin(s), end(s))));

  header h;
  h.sync_byte = u(parser, 8);
  h.transport_error_indicator = u(parser, 1);
  h.payload_unit_start_indicator = u(parser, 1);
  h.transport_priority = u(parser, 1);
  h.pid = u(parser, 13);
  h.transport_scrambling_control = u(parser, 2);
  h.adaptation_field_control = u(parser, 2);
  h.continuity_counter = u(parser, 4);

  if(h.sync_byte != 0x47) throw std::system_error(make_error_code(errc::out_of_sync));
 
  return h;
}

template<typename BS>
auto data(packet<BS> p) {
  auto h = parse_header(p);
  
  if(h.adaptation_field_control == 1)
    return split(std::move(p), begin(p) + 4).second;
  else if(h.adaptation_field_control == 2)
    return split(std::move(p), end(p)).second;
  else if(h.adaptation_field_control == 3) {
    auto adaptation_field_length = *(begin(p)+4);
    if(adaptation_field_length > 182 || adaptation_field_length < 0) throw std::system_error(make_error_code(errc::invalid_adaptation_field_length));
    return split(std::move(p), begin(p) + 5 + adaptation_field_length).second;
  }

  throw std::system_error(make_error_code(errc::invalid_adaptation_field_control_code));
}

namespace pes {

enum class errc {
  packet_start_code_prefix_not_found,
  invalid_stream_id,
  invalid_packet_length,
  missing_presentation_timestamp
};

inline
std::error_category const& error_category() {
  static struct : public std::error_category {
    const char* name() const noexcept { return "mpeg::pes"; }
 
    virtual std::string message(int ev) const {
      switch(static_cast<errc>(ev)) {
      case errc::packet_start_code_prefix_not_found: return "mpeg::pes::packet_start_code_prefix_not_found";
      default: return "unknown error";
      };
    } 
  } cat;
  return cat;
}

inline
std::error_code make_error_code(errc e) { return std::error_code(static_cast<int>(e), error_category()); }

using timestamp = std::chrono::duration<std::int64_t, std::ratio<1, 90000>>;

enum class streamid {
  program_stream_map        = 0b10111100,
  private_stream_1          = 0b10111101,
  padding_stream            = 0b10111110,
  private_stream_2          = 0b10111111,
  audio_stream_0            = 0b11000000,
  audio_stream_31           = 0b11011111,
  video_stream_0            = 0b11100000,
  video_stream_15           = 0b11101111,
  ECM                       = 0b11110000,
  EMM                       = 0b11110001,
  DSMCC_stream              = 0b11110010,
  ISO_IEC_13522_stream      = 0b11110011,
  H_222_1_type_A            = 0b11110100,
  H_222_1_type_B            = 0b11110101,
  H_222_1_type_C            = 0b11110110,
  H_222_1_type_D            = 0b11110111,
  H_222_1_type_E            = 0b11111000,
  ancillary_stream          = 0b11111001,
  SL_packetized_stream      = 0b11111010,
  FlexMux_stream            = 0b11111011,
  metadata_stream           = 0b11111100,
  extended_stream_id        = 0b11111101,
  reserved_data_stream      = 0b11111110,
  program_stream_directory  = 0b11111111
};

template<typename I>
I data(I first, I last) {
  auto parser = bitstream::make_bit_parser(bitstream::make_bit_range(utils::make_range(first, last)));

  if(u(parser, 24) != 0x1) throw std::system_error(make_error_code(errc::packet_start_code_prefix_not_found));
  
  auto stream_id = u(parser, 8);
  if(stream_id < 0xbc) throw std::system_error(make_error_code(errc::invalid_stream_id));

  auto PES_packet_length = u(parser, 16);
  if(PES_packet_length + 4 > last - first) throw std::system_error(make_error_code(errc::invalid_packet_length));
 
  switch(static_cast<streamid>(stream_id)) {
  case streamid::padding_stream:
    return last;
  case streamid::program_stream_map:
  case streamid::private_stream_2:
  case streamid::ECM:
  case streamid::EMM:
  case streamid::program_stream_directory:
  case streamid::DSMCC_stream:
  case streamid::H_222_1_type_E:
    return first + 4;
  default: {
    u(parser, 16);
    auto PES_header_data_length = u(parser, 8);
    return first + 9 + PES_header_data_length;
    }
  }
}

template<typename I>
timestamp pts(I first, I last) {
  auto parser = bitstream::make_bit_parser(bitstream::make_bit_range(utils::make_range(first, last)));

  if(u(parser, 24) != 0x1) throw std::system_error(make_error_code(errc::packet_start_code_prefix_not_found));
  
  auto stream_id = u(parser, 8);
  if(stream_id < 0xbc) throw std::system_error(make_error_code(errc::invalid_stream_id));

  auto PES_packet_length = u(parser, 16);
  if(PES_packet_length + 4 > last - first) throw std::system_error(make_error_code(errc::invalid_packet_length));
 
  switch(static_cast<streamid>(stream_id)) {
  case streamid::padding_stream:
  case streamid::program_stream_map:
  case streamid::private_stream_2:
  case streamid::ECM:
  case streamid::EMM:
  case streamid::program_stream_directory:
  case streamid::DSMCC_stream:
  case streamid::H_222_1_type_E:
    throw std::system_error(make_error_code(errc::missing_presentation_timestamp));
  default: {
    u(parser, 8);
    auto PTS_DTS_flags = u(parser, 2);
    u(parser, 14);
    
    if(PTS_DTS_flags == 0b10 || PTS_DTS_flags == 0b11) {
      u(parser, 4);
      auto t = std::int64_t(u(parser, 3)) << 30;
      u(parser, 1);
      t |= u(parser, 15) << 15;
      u(parser, 1);
      t |= u(parser, 15);
      return timestamp(t);
    }
    throw std::system_error(make_error_code(errc::missing_presentation_timestamp));
    }
  }
}

struct packet_tag {};

template<typename BS>
using packet = utils::tagged_byte_sequence<packet_tag, BS>;

template<typename BS>
auto data(packet<BS> p) {
  return split(std::move(p), data(begin(p), end(p))).second;
}

template<typename BS>
auto pts(packet<BS> const& p) {
  return pts(begin(p), end(p));
}

struct packet_assembler {
  std::vector<std::uint8_t> buffer;
  int continuity_counter;

  auto operator()() {
    return packet<std::vector<std::uint8_t>>{std::move(buffer)};
  } 

  template<typename BS>
  auto operator()(ts::packet<BS> p) {
    utils::optional<packet<std::vector<std::uint8_t>>> r;

    auto h = ts::parse_header(p); 

    if(h.adaptation_field_control != 0 && h.adaptation_field_control != 2)  
      continuity_counter = (continuity_counter + 1) % 16;
    
    if(buffer.empty() || continuity_counter != h.continuity_counter) {
      buffer.clear();
      continuity_counter = h.continuity_counter;
    }

    if(!buffer.empty() && h.payload_unit_start_indicator)
      r = utils::tag<packet_tag>(std::move(buffer));

    if(!buffer.empty() || h.payload_unit_start_indicator)
      push_back_buffer_sequence(buffer, as_asio_sequence(data(std::move(p))));

    return r;
  }
};

} // namespace pes

template<typename Source, std::size_t N>
struct buffered_reader {
  buffered_reader(Source source) : source(std::move(source)) {}

  Source source;
  std::uint8_t buffer[N*packet_length];

  std::size_t pos = 0;
  std::size_t end = 0;

  ts::packet<utils::range<const std::uint8_t*>> operator()() {
    if(pos == end) {
      end = source(asio::mutable_buffers_1(buffer, sizeof(buffer)));
      pos = 0;
      if((end - pos) % 188) throw std::system_error(make_error_code(mpeg::ts::errc::framing_error));
      if(end == pos) return utils::tag<packet_tag>(utils::range<const std::uint8_t*>{nullptr,nullptr});
    }

    auto p = pos;
    pos += 188;

    return utils::tag<packet_tag>(utils::range<const std::uint8_t*>{buffer+p, buffer + p + 188});
  }
};

template<typename Source, std::size_t N>
struct demuxer {
  demuxer(Source source, int pids[N]) : source(std::move(source)) {
    for(auto i = 0; i != channels.size(); ++i) channels[i].pid = pids[i];
  }

  using packet_type = pes::packet<std::vector<std::uint8_t>>;

  Source source;

  //static_assert(ts::is_packet<decltype(std::declval<Source>()())>, "source() must return as ts::packet<>"); 

  bool eof = false;

  struct channel {
    int pid;
    pes::packet_assembler assembler;
    utils::promise<packet_type> promise;

    template<typename BS>
    bool operator()(ts::packet<BS> const& p) {
      auto r = assembler(p);
      if(r) 
        set(std::move(*r));

      return !!r;
    }

    void eof() { 
      set(assembler());
      set({});
    }

    void set(packet_type p) {
      utils::promise<packet_type> t;
      std::swap(t, promise);
      t.set_value(std::move(p));
    }
  };

  std::array<channel, N> channels;

  void read() {
    while(!eof) {
      auto p = source();

      eof = begin(p) == end(p);
      if(eof) {
        for(auto& a: channels) a.eof();
        break;
      }

      auto pid = ts::parse_header(p).pid;

      auto i = std::find_if(begin(channels), end(channels), [=](auto& c) { return c.pid == pid; });
      if(i != end(channels) && (*i)(p)) break;
    }
  }

  friend utils::future<packet_type> pull(demuxer& d, int pid) noexcept {
    try {
      auto i = std::find_if(d.channels.begin(), d.channels.end(), [&](auto& x) { return x.pid == pid; });
      if(i == d.channels.end()) throw std::range_error("pid out of range");

      auto f = i->promise.get_future(); 
      d.read();
      return f;
    }
    catch(...) {
      return utils::make_exceptional_future<packet_type>(std::current_exception());
    }
  }
};

template<typename Source, typename... Pids>
demuxer<buffered_reader<Source, 100>, sizeof...(Pids)> make_demuxer(Source src, Pids... pids) {
  int a[] = {pids...};
  return demuxer<buffered_reader<Source, 100>, sizeof...(Pids)>(std::move(src), a);
}

} // namespace ts

}}

#endif

