#ifndef __MPEG_dcb2a881_dd48_4a95_8336_ac881a971e35_HPP__
#define __MPEG_dcb2a881_dd48_4a95_8336_ac881a971e35_HPP__

#include <exception>
#include <array>
#include <cassert>

namespace mpeg {

struct parse_error : std::exception {
  const char* what() const noexcept { return "mpeg parse error"; }
};

template<typename S>
void next_start_code(S& s) {
  while(!byte_aligned(s)) if(u(s, 1)) 
    throw parse_error();
  while(more_data(s) && next_bits(s, 24) != 0x000001) 
    if(u(s, 8)) throw parse_error();
}

enum header_codes_t {
  picture_start_code = 0,
  slice_start_code_begin = 1,
  slice_start_code_end = 0xB0,
  user_data_start_code = 0xB2,
  sequence_header_code = 0xB3,
  sequence_error_code = 0xB4,
  extension_start_code = 0xB5,
  sequence_end_code = 0xB7,
  group_start_code = 0xB8,
  system_start_codes_begin = 0xB9,
  system_start_codes_end = 0xFF
};

enum extension_ids {
  sequence_extension_id = 1,
  sequence_display_extension_id = 2,
  quant_matrix_extension_id = 3,
  copyright_extension_id = 4,
  sequence_scalable_extension_id = 5,
  picture_display_extension_id = 7,
  picture_coding_extension_id = 8,
  picture_spatial_scalable_extension_d = 9,
  picture_temporal_scalable_extension_id = 10,
  camera_parameters_extension_id = 11,
  itu_t_extension_id = 12
};

using quantiser_matrix_t = std::array<std::uint8_t, 64>;

constexpr quantiser_matrix_t default_intra_quantiser_matrix = {
   8, 16, 19, 22, 26, 27, 29, 34,
  16, 16, 22, 24, 27, 29, 34, 37,
  19, 22, 26, 27, 29, 34, 34, 38,
  22, 22, 26, 27, 29, 34, 37, 40,
  22, 26, 27, 29, 32, 35, 40, 48,
  26, 27, 29, 32, 35, 40, 48, 58,
  26, 27, 29, 34, 38, 46, 56, 69,
  27, 29, 35, 38, 46, 56, 69, 83
};

constexpr quantiser_matrix_t default_non_intra_quantiser_matrix = {
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16,
  16, 16, 16, 16, 16, 16, 16, 16
};

struct sequence_header_t {
  std::uint16_t horizontal_size_value;
  std::uint16_t  vertical_size_value;
  unsigned aspect_ratio_information;
  unsigned frame_rate_code;
  unsigned bit_rate_value;
  bool marker_bit;
  unsigned vbv_buffer_size_value;
  bool constrained_parameters_flag;
  bool load_intra_quantiser_matrix;
  quantiser_matrix_t intra_quantiser_matrix;
  bool load_non_intra_quantiser_matrix;
  quantiser_matrix_t  non_intra_quantiser_matrix;
};

template<typename S>
sequence_header_t sequence_header(S&& s) {
  sequence_header_t h;
  h.horizontal_size_value = u(s, 12);
  h.vertical_size_value = u(s, 12);
  h.aspect_ratio_information = u(s, 4);
  h.frame_rate_code = u(s, 4);
  h.bit_rate_value = u(s, 18);
  h.marker_bit = u(s, 1);
  h.vbv_buffer_size_value = u(s, 10);
  h.constrained_parameters_flag = u(s,1);
  
  h.load_intra_quantiser_matrix = u(s,1);
  if(h.load_intra_quantiser_matrix)
    for(auto& i: h.intra_quantiser_matrix)
      i = u(s, 8);

  h.load_non_intra_quantiser_matrix = u(s, 1);
  if(h.load_non_intra_quantiser_matrix)
    for(auto& i: h.non_intra_quantiser_matrix)
      i = u(s, 8);

  return h;
}


struct group_of_pictures_header_t {
  unsigned time_code;
  bool closed_gop;
  bool broken_link;
};

template<typename S>
group_of_pictures_header_t group_of_pictures_header(S&& s) {
  group_of_pictures_header_t h;
  h.time_code = u(s, 25);
  h.closed_gop = u(s, 1);
  h.broken_link = u(s, 1);
  next_start_code(s);
  return h;
}

enum class picture_type : unsigned { top = 1, bot = 2, frame = 3};
enum class picture_coding : unsigned { I = 1, P = 2, B = 3 };

inline bool is_opposite(picture_type a, picture_type b) { 
  if(a == picture_type::top) return b == picture_type::bot;
  if(a == picture_type::bot) return b == picture_type::top;
}

struct picture_header_t {
  unsigned temporal_reference;
  picture_coding picture_coding_type;
  unsigned vbv_delay;
  bool full_pel_forward_vector;
  unsigned forward_f_code;
  bool full_pel_backward_vector;
  unsigned backward_f_code;
};

template<typename S>
picture_header_t picture_header(S&& s) {
  picture_header_t h;
  h.temporal_reference = u(s, 10);
  
  auto pic_code = u(s, 3);
  if(pic_code < 1 || pic_code > 3) throw parse_error();
  h.picture_coding_type = picture_coding(pic_code);
  
  h.vbv_delay = u(s, 16);
  
  h.full_pel_forward_vector = 0;
  if(h.picture_coding_type != picture_coding::I) {
    h.full_pel_forward_vector = u(s, 1);
    h.forward_f_code = u(s, 3);
  }

  h.full_pel_backward_vector = 0;
  if(h.picture_coding_type == picture_coding::B) {
    h.full_pel_backward_vector = u(s, 1);
    h.backward_f_code = u(s, 3);
  }

  while(u(s, 1)) u(s, 8);

  next_start_code(s);

  return h;
}

struct picture_coding_extension_t {
  uint8_t f_code[2][2];
  unsigned intra_dc_precision;
  picture_type picture_structure;
  bool top_field_first;
  bool frame_pred_frame_dct;
  bool concealment_motion_vectors;
  bool q_scale_type;
  bool intra_vlc_format;
  bool alternate_scan;
  bool repeat_first_field;
  bool chroma_420_type;
  bool progressive_frame;
  bool composite_display_flag;
  bool v_axis;
  unsigned field_sequence;
  bool sub_carrier;
  unsigned burst_amplitude;
  unsigned sub_carrier_phase;
};

template<typename S>
picture_coding_extension_t picture_coding_extension(S&& s) {
  picture_coding_extension_t x;
  x.f_code[0][0] = u(s, 4);
  x.f_code[0][1] = u(s, 4);
  x.f_code[1][0] = u(s, 4);
  x.f_code[1][1] = u(s, 4);

  x.intra_dc_precision = u(s, 2);
  x.picture_structure = static_cast<picture_type>(u(s, 2));
  
  x.top_field_first = u(s, 1);
  x.frame_pred_frame_dct = u(s, 1);
  x.concealment_motion_vectors = u(s, 1);
  x.q_scale_type = u(s, 1);
  x.intra_vlc_format = u(s, 1);
  x.alternate_scan = u(s, 1);
  x.repeat_first_field = u(s, 1);
  x.chroma_420_type = u(s, 1);
  x.progressive_frame = u(s, 1);
  x.composite_display_flag = u(s, 1);
  if(x.composite_display_flag) {
    x.v_axis = u(s, 1);
    x.field_sequence = u(s, 3);
    x.sub_carrier = u(s, 1);
    x.burst_amplitude = u(s, 7);
    x.sub_carrier_phase = u(s, 8);
  }
  next_start_code(s);
  return x;
}

struct quant_matrix_extension_t {
  bool load_intra_quantiser_matrix;
  quantiser_matrix_t intra_quantiser_matrix;
  bool load_non_intra_quantiser_matrix;
  quantiser_matrix_t non_intra_quantiser_matrix;
  bool load_chroma_intra_quantiser_matrix;
  quantiser_matrix_t chroma_intra_quantiser_matrix;
  bool load_chroma_non_intra_quantiser_matrix;
  quantiser_matrix_t chroma_non_intra_quantiser_matrix;
};

template<typename S>
quant_matrix_extension_t quant_matrix_extension(S&& s) {
  quant_matrix_extension_t x;
  
  x.load_intra_quantiser_matrix = u(s, 1);
  for(auto& i: x.intra_quantiser_matrix)
    i = u(s, 8);

  x.load_non_intra_quantiser_matrix = u(s, 1);
  for(auto& i: x.non_intra_quantiser_matrix)
    i = u(s, 8);

  x.load_chroma_intra_quantiser_matrix = u(s, 1);
  for(auto& i: x.chroma_intra_quantiser_matrix)
    i = u(s, 8);

  x.load_chroma_intra_quantiser_matrix = u(s, 1);
  for(auto& i: x.chroma_non_intra_quantiser_matrix)
    i = u(s, 8);

  next_start_code(s);
  
  return x;
}

template<typename S>
void unknown_extension(S& s) {
  u(s, 4);

  assert(byte_aligned(s));

  while(next_bits(s, 24) != 1)
    u(s, 8);
}

template<typename S>
void unknown_high_level_syntax_element(S&& s) {
  assert(byte_aligned(s));

  while(more_data(s) && next_bits(s, 24) != 1)
    u(s, 8);
}

}

#endif

