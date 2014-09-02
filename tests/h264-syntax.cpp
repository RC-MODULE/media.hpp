#include "../h264-syntax.hpp"

using namespace h264;

int main(int argc, char* argv[]) {
  int fd = open(argv[1], O_RDONLY);
  auto stream = make_stream([fd](std::uint8_t* buffer, std::size_t n) {
    auto r = read(fd, buffer, n); 
    if(r < 0) throw std::system_error(errno, std::system_category());
    return r; 
  });

  parsing_context cx;

  while(stream) {
    auto nalu = rbsp(read_nalu(stream));
    
    auto h = parse_nal_unit_header(nalu);

    switch(static_cast<nalu_type>(h.nal_unit_type)) {
    case nalu_type::seq_parameter_set:
      add(cx, parse_sps(nalu));
      break;
    case nalu_type::pic_parameter_set:
      add(cx, parse_pps(cx, nalu));
      break;
    case nalu_type::slice_layer_non_idr:
    case nalu_type::slice_layer_idr:
      parse_slice_header(cx, nalu, h.nal_unit_type, h.nal_ref_idc);
      break;
    }
  } 
}

