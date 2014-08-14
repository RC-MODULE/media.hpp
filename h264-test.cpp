#include "h264.hpp"

using namespace h264;

std::ostream& operator << (std::ostream& os, picture_type pt) {
  return os << (pt == picture_type::frame ? "F" : (pt == picture_type::top ? "T" : "B"));
}
std::ostream& operator << (std::ostream& os, ref_type rt) {
  return os << (rt == ref_type::short_term ? "short" : rt == ref_type::long_term ? "long" : "none");
}

template<typename Buffer> 
std::ostream& operator << (std::ostream& os, picture<Buffer> const& p) {
  return os << "{" << p.pt << "," << p.frame_num << "," << PicOrderCnt(p) << "," << p.rt << "}";
}

template<typename Buffer>
picture_type fields_to_pic_type(stored_frame<Buffer> const& f) { 
  if(f.top.valid) return f.bot.valid ? picture_type::frame: picture_type::top;
  return picture_type::bot;
}

template<typename Buffer> 
std::ostream& operator << (std::ostream& os, stored_frame<Buffer> const& p) {
  return os << "{" << fields_to_pic_type(p) << "," << p.frame_num << ",{" << unsigned(p.top.poc) << "," << p.top.rt << "," << p.long_term_frame_idx << "},{" 
      << unsigned(p.bot.poc) << "," << p.bot.rt << "," << p.long_term_frame_idx << "}}";
}

std::ostream& operator << (std::ostream& os, memory_management_control_operation const& op) {
  std::cout << "{id:" << op.id;
  if(op.id == 1 || op.id == 3)
    std::cout << " difference_of_pic_nums_minus1:" << op.difference_of_pic_nums_minus1;
  if(op.id == 2)
    std::cout << " long_term_pic_num:" << op.long_term_pic_num;
  if(op.id == 3 || op.id == 6)
    std::cout << " long_term_frame_idx:" << op.long_term_frame_idx;
  if(op.id == 4)
    std::cout << " max_long_term_frame_idx_plus1:" << op.max_long_term_frame_idx_plus1;

  return std::cout << "}";
};

std::ostream& operator << (std::ostream& os, std::pair<unsigned, unsigned> const& op) {
  return os << "{" << op.first << "," << op.second << "}";
}

template<typename T>
std::ostream& operator << (std::ostream& os, std::vector<T> const& v) {
  os << "{";
  bool first = true;
  for(auto& a: v) {
    if(!first) os << ",";
    first = false;
    os << a;
  }
  return os << "}";
}

int main(int argc, char* argv[]) {
  int fd = open(argv[1], O_RDONLY);
  auto stream = make_stream([fd](std::uint8_t* buffer, std::size_t n) {
    auto r = read(fd, buffer, n); 
    if(r < 0) throw std::system_error(errno, std::system_category());
    return r; 
  });

  context<std::uint32_t> ctx;
  std::uint32_t n = 0;
  std::vector<sequence_parameter_set> spss;
  std::vector<picture_parameter_set> ppss;

  std::uint32_t poc = -1u;

  while(stream) {
    auto nalu = read_nalu(stream);
    auto nalu_size = nalu.second - nalu.first;  
    auto parser = rbsp(std::ref(nalu));

    u(parser, 24);
    u(parser, 1);
    
    auto nal_ref_idc = u(parser, 2);
    auto nal_unit_type = u(parser, 5); 
 
    switch(nal_unit_type) {
    case 7:
      add(spss, parse_sps(parser));
      break;
    case 8:
      add(ppss, parse_pps(parser, spss));
      break;
    case 5:
    case 1:
      if(ctx.process_slice_header(parser, ppss, nal_unit_type, nal_ref_idc)) {
        if(ctx.IdrPicFlag) std::cout << "IDR" << std::endl;

        if(ctx.curr_pic->buffer == 0) ctx.curr_pic->buffer = ++n;
        
        if(PicOrderCnt(*ctx.curr_pic) != poc) {
          if(ctx.IdrPicFlag) std::cout << "IDR" << std::endl;
          std::cout << "pic_order_cnt_lsb:" << ctx.pic_order_cnt_lsb << std::endl;
          std::cout << "mmos:" << ctx.mmcos << std::endl;
          poc = PicOrderCnt(*ctx.curr_pic);
          std::cout << "dpb:" << ctx.dpb << std::endl;
          std::cout << "new picture:" << *ctx.curr_pic << std::endl;
        }

        std::cout << "new_slice: {" << (ctx.slice_type == slice_coding::I ? "I" : ( ctx.slice_type == slice_coding::P ? "P" : "B"))
          << "," << ctx.first_mb_in_slice << "}" << std::endl;

        if(ctx.slice_type != slice_coding::I) {
          auto reflists = ctx.construct_reflists();
          if(!reflists[0].empty()) std::cout << "reflist0:" << reflists[0] << std::endl;
          if(!reflists[1].empty()) std::cout << "reflist1:" << reflists[1] << std::endl;
        }

        ctx.dpb.erase(std::remove_if(ctx.dpb.begin(), ctx.dpb.end(), [&](stored_frame<std::uint32_t> const& f) { return !is_reference(f.top) && !is_reference(f.bot); }),
                  ctx.dpb.end());
      }
      break;
    };

    stream.advance(nalu_size);
  }
}

