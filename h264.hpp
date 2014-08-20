#ifndef __h264_hpp__61023805_eed5_4faf_b336_2267b0960b51__
#define __h264_hpp__61023805_eed5_4faf_b336_2267b0960b51__

#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
#include <numeric>
#include <cstring>

#include "utils.hpp"
#include "bitstream.hpp"

namespace h264 {
enum class picture_type { frame, top, bot };

inline bool has_top(picture_type pt) { return pt == picture_type::frame || pt == picture_type::top; }
inline bool has_bot(picture_type pt) { return pt == picture_type::frame || pt == picture_type::bot; }

enum class coding_type { I, P, B };

inline
coding_type cast_to_coding_type(std::uint32_t t) {
  switch(t % 5) {
  case 0: return coding_type::P;
  case 1: return coding_type::B;
  case 2: return coding_type::I;
  default: throw std::runtime_error("Unsupported slice type");
  }
}

enum class ref_type { none, short_term, long_term };

struct memory_management_control_operation {
  unsigned id;
  union {
    struct {
      union {
        unsigned difference_of_pic_nums_minus1;
        unsigned long_term_pic_num;
      };

      unsigned long_term_frame_idx;
    };
    unsigned max_long_term_frame_idx_plus1;
  };
};

struct scaling_list {
  std::uint8_t list4x4[6][16];
  std::uint8_t list8x8[6][64];
};

struct sequence_parameter_set {
  unsigned  profile_idc;
  bool      constrained_set0_flag = false;
  bool      constrained_set1_flag = false;
  bool      constrained_set2_flag = false;
  bool      constrained_set3_flag = false;
  bool      constrained_set4_flag = false;
  bool      constrained_set5_flag = false;
  unsigned  level_idc;
  unsigned  seq_parameter_set_id = -1u;
  
  unsigned  chroma_format_idc = 1; //4:2:0 by default
  bool      separate_colour_plane_flag = false;
  unsigned  bit_depth_luma_minus8;
  unsigned  bit_depth_chroma_minus8;
  bool      qpprime_y_zero_transform_bypass_flag = false;

  bool      seq_scaling_matrix_present_flag = false;
  std::array<std::array<std::uint8_t, 16>,6> scaling_lists_4x4;
  std::array<std::array<std::uint8_t, 64>,6> scaling_lists_8x8;

  unsigned  log2_max_frame_num_minus4;
  
  unsigned  pic_order_cnt_type;
  // if( pic_order_cnt_type == 0 )
  unsigned log2_max_pic_order_cnt_lsb_minus4;
  // else if( pic_order_cnt_type == 1 )
  bool      delta_pic_order_always_zero_flag = false;
  int       offset_for_non_ref_pic;
  int       offset_for_top_to_bottom_field;
  std::vector<int> offset_for_ref_frame;
  
  unsigned  max_num_ref_frames;
  bool      gaps_in_frame_num_value_allowed_flag = false;
  unsigned  pic_width_in_mbs_minus1;
  unsigned  pic_height_in_map_units_minus1;
  bool      frame_mbs_only_flag = false;
  // if(!frame_mbs_only_flag)
  bool      mb_adaptive_frame_field_flag = false;
 
  bool      direct_8x8_inference_flag = false;
  bool      frame_cropping_flag = false;
  //if(frame_cropping_flag)
  unsigned  frame_crop_left_offset = 0;
  unsigned  frame_crop_right_offset = 0;
  unsigned  frame_crop_top_offset = 0;
  unsigned  frame_crop_bottom_offset =0;
};


inline
std::vector<sequence_parameter_set>& add(std::vector<sequence_parameter_set>& spss, sequence_parameter_set&& sps) {
  spss.resize(std::max(spss.size(), std::size_t(sps.seq_parameter_set_id + 1)));
  spss[sps.seq_parameter_set_id] = sps;
  return spss;
}

template<typename Source, std::size_t I>
inline
void parse_scaling_list(Source& a, std::array<uint8_t, I>& list, bool& use_default) {
  std::uint8_t next = 8;
  std::uint8_t last = 8;

  for(auto& x: list) {
    if(next) {
      auto delta = se(a);
      next = (last + delta + 256) % 256;
      use_default = &x == &list[0] && next == 0;
    }

    x = (next == 0) ? last : next;
    last = x;
  }
}

template<typename Source, std::size_t I>
inline
void parse_scaling_list(Source& a, std::array<std::uint8_t, I> const& fallback, std::array<std::uint8_t, I> const& def, std::array<std::uint8_t, I>& list) {
  auto scaling_list_present_flag = u(a, 1);
  if(scaling_list_present_flag) {
    bool use_default = false;
    parse_scaling_list(a, list, use_default);
    if(use_default) list = def;
  }
  else
    list = fallback;
}

constexpr std::array<std::uint8_t, 16> default_4x4_intra{6,13,13,20,20,20,28,28,28,28,32,32,32,37,37,42};
constexpr std::array<std::uint8_t, 16> default_4x4_inter = {10,14,14,20,20,20,24,24,24,24,27,27,27,30,30,34};
constexpr std::array<std::uint8_t, 64> default_8x8_intra = {
   6,10,10,13,11,13,16,16,16,16,18,18,18,18,18,23,
  23,23,23,23,23,25,25,25,25,25,25,25,27,27,27,27,
  27,27,27,27,29,29,29,29,29,29,29,31,31,31,31,31,
  31,33,33,33,33,33,36,36,36,36,38,38,38,40,40,42
};
constexpr std::array<std::uint8_t, 64> default_8x8_inter = {
   9,13,13,15,13,15,17,17,17,17,19,19,19,19,19,21,
  21,21,21,21,21,22,22,22,22,22,22,22,24,24,24,24,
  24,24,24,24,25,25,25,25,25,25,25,27,27,27,27,27,
  27,28,28,28,28,28,30,30,30,30,32,32,32,33,33,35
};

constexpr std::array<std::array<std::uint8_t, 16>, 6> default_scaling_lists_4x4 = {default_4x4_intra, default_4x4_intra, default_4x4_intra,
  default_4x4_inter, default_4x4_inter, default_4x4_inter};

constexpr std::array<std::array<std::uint8_t, 64>, 6> default_scaling_lists_8x8 = {default_8x8_intra,default_8x8_intra,default_8x8_intra,
  default_8x8_inter,default_8x8_inter,default_8x8_inter};

template<typename Source>
inline
void parse_scaling_lists(Source& a, unsigned chroma_idc,
  std::array<std::array<std::uint8_t, 16>, 6> const& fallback4x4,
  std::array<std::array<std::uint8_t, 64>, 6> const& fallback8x8,
  std::array<std::array<std::uint8_t, 16>, 6>& list4x4,
  std::array<std::array<std::uint8_t, 64>, 6>& list8x8)
{
  for(int i = 0; i != 6; ++i)
    parse_scaling_list(a, (i == 0 || i == 3) ? fallback4x4[i] : list4x4[i-1], default_scaling_lists_4x4[i], list4x4[i]);

  for(int i = 0; i != (chroma_idc != 3 ? 2: 6); ++i)
    parse_scaling_list(a, (i == 0 || i == 1) ? fallback8x8[i] : list8x8[i-2], default_scaling_lists_8x8[i], list8x8[i]);
}

template<typename Source>
sequence_parameter_set parse_sps(Source& a) {
  sequence_parameter_set sps;
  sps.profile_idc = u(a, 8);
  sps.constrained_set0_flag = u(a, 1);
  sps.constrained_set1_flag = u(a, 1);
  sps.constrained_set2_flag = u(a, 1);
  sps.constrained_set3_flag = u(a, 1);
  sps.constrained_set4_flag = u(a, 1);
  sps.constrained_set5_flag = u(a, 1);

  u(a, 2);

  sps.level_idc = u(a, 8);
  sps.seq_parameter_set_id = ue(a);

  memset(&sps.scaling_lists_4x4, 0x10, sizeof(sps.scaling_lists_4x4));
  memset(&sps.scaling_lists_8x8, 0x10, sizeof(sps.scaling_lists_8x8));

  if(sps.profile_idc == 100 || sps.profile_idc == 110 ||
      sps.profile_idc == 122 || sps.profile_idc == 244 || sps.profile_idc == 44 ||
      sps.profile_idc == 83 || sps.profile_idc == 86 || sps.profile_idc == 118 ||
      sps.profile_idc == 128 || sps.profile_idc == 138)
  {
    sps.chroma_format_idc = ue(a);
    if(sps.chroma_format_idc == 3)
      sps.separate_colour_plane_flag = u(a, 1);
  
    sps.bit_depth_luma_minus8 = ue(a);
    sps.bit_depth_chroma_minus8 = ue(a);
    sps.qpprime_y_zero_transform_bypass_flag = u(a, 1);
    sps.seq_scaling_matrix_present_flag = u(a, 1);
    if(sps.seq_scaling_matrix_present_flag) {
      parse_scaling_lists(a, sps.chroma_format_idc, default_scaling_lists_4x4, default_scaling_lists_8x8, sps.scaling_lists_4x4, sps.scaling_lists_8x8);
    }
  }

  sps.log2_max_frame_num_minus4 = ue(a);
  sps.pic_order_cnt_type = ue(a);
  if(sps.pic_order_cnt_type == 0)
    sps.log2_max_pic_order_cnt_lsb_minus4 = ue(a);
  else if(sps.pic_order_cnt_type == 1) {
    sps.delta_pic_order_always_zero_flag = u(a, 1);
    sps.offset_for_non_ref_pic = se(a);
    sps.offset_for_top_to_bottom_field = se(a);
    sps.offset_for_ref_frame.resize(ue(a));
    for(auto& x: sps.offset_for_ref_frame) x = se(a);
  }

  sps.max_num_ref_frames = ue(a);
  sps.gaps_in_frame_num_value_allowed_flag = u(a, 1);
  sps.pic_width_in_mbs_minus1 = ue(a);
  sps.pic_height_in_map_units_minus1 = ue(a);
  sps.frame_mbs_only_flag = u(a, 1);
  if(!sps.frame_mbs_only_flag)
    sps.mb_adaptive_frame_field_flag = u(a, 1);
  sps.direct_8x8_inference_flag = u(a, 1);
  sps.frame_cropping_flag = u(a, 1);
  if(sps.frame_cropping_flag) {
    sps.frame_crop_left_offset = ue(a);
    sps.frame_crop_right_offset = ue(a);
    sps.frame_crop_top_offset = ue(a);
    sps.frame_crop_bottom_offset = ue(a);
  }
  // vui_parameter_present
  return sps;
}


struct picture_parameter_set : sequence_parameter_set {
  unsigned  pic_parameter_set_id = -1u;
  //unsigned  seq_parameter_set_id;
  bool      entropy_coding_mode_flag = false;
  bool      bottom_field_pic_order_in_frame_present_flag = false;
  unsigned  num_slice_groups_minus1 = 0;

  unsigned  num_ref_idx_l0_default_active_minus1;
  unsigned  num_ref_idx_l1_default_active_minus1;
  bool      weighted_pred_flag = false;
  unsigned  weighted_bipred_idc = 0;
  int       pic_init_qp_minus26;
  int       pic_init_qs_minus26;
  int       chroma_qp_index_offset;
  int       second_chroma_qp_index_offset;
  bool      deblocking_filter_control_present_flag = false;
  bool      constrained_intra_pred_flag = false;
  bool      redundant_pic_cnt_present_flag = false;

  bool      transform_8x8_mode_flag = false;
  bool      pic_scaling_matrix_present_flag = false;

  bool is_valid() const { return pic_parameter_set_id != -1u; }
};

inline
std::vector<picture_parameter_set>& add(std::vector<picture_parameter_set>& ppss, picture_parameter_set&& pps) {
  ppss.resize(std::max(ppss.size(), std::size_t(pps.pic_parameter_set_id + 1)));
  ppss[pps.pic_parameter_set_id] = pps;
  return ppss;
}

inline unsigned ChromaArrayType(picture_parameter_set const& pps) {
  return pps.separate_colour_plane_flag == 0 ? pps.chroma_format_idc : 0;
} 

template<typename Source>
picture_parameter_set parse_pps(Source& a, std::vector<sequence_parameter_set> const& spss) {
  picture_parameter_set pps;

  pps.pic_parameter_set_id = ue(a);
  pps.seq_parameter_set_id = ue(a);
  static_cast<sequence_parameter_set&>(pps) = spss.at(pps.seq_parameter_set_id);

  pps.entropy_coding_mode_flag = u(a,1);
  pps.bottom_field_pic_order_in_frame_present_flag = u(a,1);

  pps.num_slice_groups_minus1 = ue(a);
  if(pps.num_slice_groups_minus1 > 0) throw std::runtime_error("num_slice_groups_minus1 > 0 is not supported");

  pps.num_ref_idx_l0_default_active_minus1 = ue(a);
  pps.num_ref_idx_l1_default_active_minus1 = ue(a);

  pps.weighted_pred_flag = u(a,1);
  pps.weighted_bipred_idc = u(a, 2);

  pps.pic_init_qp_minus26 = se(a);
  pps.pic_init_qs_minus26 = se(a);
  pps.second_chroma_qp_index_offset = pps.chroma_qp_index_offset = se(a);

  pps.deblocking_filter_control_present_flag = u(a,1);
  pps.constrained_intra_pred_flag = u(a,1);
  pps.redundant_pic_cnt_present_flag = u(a,1);

  if(more_rbsp_data(a)) {
    pps.transform_8x8_mode_flag = u(a,1);
    pps.pic_scaling_matrix_present_flag = u(a,1);
    if(pps.pic_scaling_matrix_present_flag) {
      if(pps.seq_scaling_matrix_present_flag)
        parse_scaling_lists(a, pps.chroma_format_idc, pps.scaling_lists_4x4, pps.scaling_lists_8x8, pps.scaling_lists_4x4, pps.scaling_lists_8x8);
      else
        parse_scaling_lists(a, pps.chroma_format_idc, default_scaling_lists_4x4, default_scaling_lists_8x8, pps.scaling_lists_4x4, pps.scaling_lists_8x8);
    }
    pps.second_chroma_qp_index_offset = se(a);
  }

  return pps;
}

// slice_header with enough information to distinguish between different pictures
struct slice_partial_header {
  bool IdrPicFlag;
  unsigned nal_ref_idc;

  unsigned first_mb_in_slice;
  coding_type slice_type;
  unsigned pic_parameter_set_id = -1u;
  picture_type pic_type;
  unsigned idr_pic_id;

  unsigned frame_num;
  unsigned colour_plane_id = 0;

  union {
    struct  {
      unsigned pic_order_cnt_lsb;
      unsigned delta_pic_order_cnt_bottom;
    };
    int delta_pic_order_cnt[2] = {0,0};
  };
};

// 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
inline
bool is_new_picture(slice_partial_header const& a, slice_partial_header const& b) {
  return a.pic_type != b.pic_type
        || a.pic_parameter_set_id != b.pic_parameter_set_id
        || a.frame_num != b.frame_num
        || (a.nal_ref_idc != b.nal_ref_idc && (a.nal_ref_idc == 0 || b.nal_ref_idc == 0))
        || a.IdrPicFlag != b.IdrPicFlag
        || (a.IdrPicFlag && (a.idr_pic_id != b.idr_pic_id))
        || a.pic_order_cnt_lsb != b.pic_order_cnt_lsb || a.delta_pic_order_cnt_bottom != b.delta_pic_order_cnt_bottom;
}

struct slice_header : slice_partial_header {
  unsigned redundant_pic_cnt;
  bool direct_spatial_mv_pred_flag = false;
  bool num_ref_idx_active_override_flag = false;
  unsigned num_ref_idx_l0_active_minus1;
  unsigned num_ref_idx_l1_active_minus1;

  std::vector<std::pair<unsigned, unsigned>> ref_pic_list_modification[2];

  unsigned luma_log2_weight_denom = 0;
  unsigned chroma_log2_weight_denom = 0;

  struct weight_pred_table_element {
    std::int8_t weight;
    std::int8_t offset;
  };

  struct chroma_weight_pred_table_element {
    weight_pred_table_element cb;
    weight_pred_table_element cr;
  };

  struct {
    std::vector<weight_pred_table_element> luma;
    std::vector<chroma_weight_pred_table_element> chroma;
  } weight_pred_table[2];

  bool no_output_of_prior_pics_flag = false;
  bool long_term_reference_flag = false;
  std::vector<memory_management_control_operation> mmcos;

  unsigned cabac_init_idc = 3; // msvd expects cabac_init_idc for i slices
  int slice_qp_delta = 0;

  unsigned disable_deblocking_filter_idc = 0;
  int slice_alpha_c0_offset_div2 = 0;
  int slice_beta_offset_div2 = 0;
};

template<typename Source>
void parse_slice_header_up_to_pps_id(Source& a, unsigned nal_unit_type, unsigned nal_ref_idc, slice_partial_header& slice) {
  slice.IdrPicFlag = nal_unit_type == 5;
  slice.nal_ref_idc = nal_ref_idc;

  slice.first_mb_in_slice = ue(a);
  slice.slice_type = cast_to_coding_type(ue(a));
  slice.pic_parameter_set_id = ue(a);
}

template<typename Source>
void parse_slice_partial_header_after_pps_id(Source& a, picture_parameter_set const& pps, slice_partial_header& slice) {
  if(pps.separate_colour_plane_flag)
    slice.colour_plane_id = ue(a);

  slice.frame_num = u(a, pps.log2_max_frame_num_minus4+4);
 
  slice.pic_type = picture_type::frame;
  if(!pps.frame_mbs_only_flag) {
    if(u(a,1))
      slice.pic_type = u(a,1) ? picture_type::bot : picture_type::top;
  }

  if(slice.IdrPicFlag)
    slice.idr_pic_id = ue(a);

  if(pps.pic_order_cnt_type == 0) {
    slice.pic_order_cnt_lsb = u(a, pps.log2_max_pic_order_cnt_lsb_minus4 + 4);
    slice.delta_pic_order_cnt_bottom = 0;
    if(pps.bottom_field_pic_order_in_frame_present_flag && slice.pic_type == picture_type::frame)
      slice.delta_pic_order_cnt_bottom = se(a);
  }
  else if(pps.pic_order_cnt_type == 1 && !pps.delta_pic_order_always_zero_flag) {
    slice.delta_pic_order_cnt[0] = se(a);
    slice.delta_pic_order_cnt[1] = (pps.bottom_field_pic_order_in_frame_present_flag && slice.pic_type == picture_type::frame) ? se(a) : 0;
  }
  else {
    slice.delta_pic_order_cnt[0] = slice.delta_pic_order_cnt[1] = 0;
  }
}

template<typename Source>
inline
bool parse_slice_header_rest(Source& a, picture_parameter_set const& pps, slice_header& slice) {
  slice.direct_spatial_mv_pred_flag =  slice.slice_type == coding_type::B ? u(a, 1) : false;

  if(slice.slice_type == coding_type::P || slice.slice_type == coding_type::B) {
    slice.num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
    slice.num_ref_idx_l1_active_minus1 = pps.num_ref_idx_l1_default_active_minus1;
    slice.num_ref_idx_active_override_flag = u(a, 1);
    if(slice.num_ref_idx_active_override_flag) {
      slice.num_ref_idx_l0_active_minus1 = ue(a);
      if(slice.slice_type == coding_type::B)
        slice.num_ref_idx_l1_active_minus1 = ue(a);
    }
  }

  auto ref_pic_list_modification = [&](std::vector<std::pair<unsigned, unsigned>>& ops) {
    ops.clear();
    for(;;) {
      auto modification_of_pic_nums_idc = ue(a);
      if(modification_of_pic_nums_idc == 3) break;

      ops.push_back({modification_of_pic_nums_idc, ue(a)});
    }
  };

  slice.ref_pic_list_modification[0].clear();
  slice.ref_pic_list_modification[1].clear();

  if(slice.slice_type != coding_type::I)
    if(u(a,1)) //ref_pic_list_modification_flag_l0
      ref_pic_list_modification(slice.ref_pic_list_modification[0]);

  if(slice.slice_type == coding_type::B)
    if(u(a,1)) // ref_pic_list_modification_flag_l1
      ref_pic_list_modification(slice.ref_pic_list_modification[1]);

  if((pps.weighted_pred_flag && slice.slice_type == coding_type::P) || (pps.weighted_bipred_idc == 1 && slice.slice_type == coding_type::B)) {
    slice.luma_log2_weight_denom = ue(a);
    if(ChromaArrayType(pps) != 0)
      slice.chroma_log2_weight_denom = ue(a);

    auto read_weight_pred_table = [&](int n, decltype(slice.weight_pred_table[0])& wt) {
      wt.luma.resize(n);
      if(ChromaArrayType(pps) != 0) wt.chroma.resize(n);

      for(int i = 0; i != n; ++i) {
        wt.luma[i].weight = 1 << slice.luma_log2_weight_denom;
        wt.luma[i].offset = 0;

        if(u(a,1)) {
          wt.luma[i].weight = se(a);
          wt.luma[i].offset = se(a);
        }
        
        if(ChromaArrayType(pps) != 0) {
          wt.chroma[i].cb.weight = wt.chroma[i].cr.weight = 1 << slice.chroma_log2_weight_denom;
          wt.chroma[i].cb.offset = wt.chroma[i].cr.offset = 0;
  
          if(u(a,1)) {
            wt.chroma[i].cb.weight = se(a);
            wt.chroma[i].cb.offset = se(a);
            wt.chroma[i].cr.weight = se(a);
            wt.chroma[i].cr.offset = se(a);
          }
        }
      }
    };

    read_weight_pred_table(slice.num_ref_idx_l0_active_minus1+1, slice.weight_pred_table[0]);
    if(slice.slice_type == coding_type::B)
      read_weight_pred_table(slice.num_ref_idx_l1_active_minus1+1, slice.weight_pred_table[1]);
  }

  if(slice.nal_ref_idc) {
    slice.mmcos.clear();
    if(slice.IdrPicFlag) {
      slice.no_output_of_prior_pics_flag = u(a, 1);
      slice.long_term_reference_flag = u(a, 1);
    }
    else if(u(a,1)) {
      for(;;) {
        memory_management_control_operation mmco;
        mmco.id = ue(a);
        if(mmco.id == 0) break;

        if(mmco.id == 1 || mmco.id == 3) mmco.difference_of_pic_nums_minus1 = ue(a);
        if(mmco.id == 2) mmco.long_term_pic_num = ue(a);
        if(mmco.id == 3 || mmco.id == 6) mmco.long_term_frame_idx = ue(a);
        if(mmco.id == 4) mmco.max_long_term_frame_idx_plus1 = ue(a);

        slice.mmcos.push_back(mmco);
      }
    }
  }

  slice.cabac_init_idc = 3;
  if(pps.entropy_coding_mode_flag && slice.slice_type != coding_type::I)
    slice.cabac_init_idc = ue(a);

  slice.slice_qp_delta = se(a);

  if(pps.deblocking_filter_control_present_flag) {
    slice.disable_deblocking_filter_idc = ue(a);
    if(slice.disable_deblocking_filter_idc != 1) {
      slice.slice_alpha_c0_offset_div2 = se(a);
      slice.slice_alpha_c0_offset_div2 = se(a);
    }
  }
  return true;
}
 
template<typename Buffer>
struct stored_frame;

struct poc_t {
  int top;
  int bot;
};

template<typename Buffer>
struct stored_picture {
  picture_type pt;
  
  stored_frame<Buffer> const& frame() const;
  stored_frame<Buffer>& frame();

  poc_t poc() const;

  Buffer const& buffer() const;

  ref_type rt() const;
  void rt(ref_type);
private:
  stored_picture(picture_type pt) : pt(pt) {}
  
  stored_picture(stored_picture const&) = default;
  stored_picture(stored_picture&&) = default;
  stored_picture& operator=(stored_picture const&) = default;
  stored_picture& operator=(stored_picture&&) = default;

  template<typename> friend struct stored_frame;
  template<typename> friend struct stored_field;
};

template<typename Buffer>
struct stored_field : stored_picture<Buffer> {  
  int poc;
  bool valid = false;

  ref_type rt = ref_type::none;
private:
  stored_field(picture_type pt, bool valid = false, unsigned poc = 0) : stored_picture<Buffer>(pt), poc(poc), valid(valid) {}

  stored_field(stored_field const&) = default;
  stored_field(stored_field&&) = default;
  stored_field& operator=(stored_field const&) = default;
  stored_field& operator=(stored_field&&) = default;

  template<typename> friend struct stored_frame;
};

template<typename Buffer>
struct stored_frame : stored_picture<Buffer> {
  stored_field<Buffer> top = {picture_type::top};
  stored_field<Buffer> bot = {picture_type::bot};

  Buffer buffer;
  std::uint32_t frame_num;
  std::uint32_t long_term_frame_idx;

  stored_frame() : stored_picture<Buffer>(picture_type::frame) {}
  stored_frame(stored_frame const&) = default;
  stored_frame(stored_frame&&) = default;

  stored_frame& operator=(stored_frame const&) = default;
  stored_frame& operator=(stored_frame&&) = default;  
};

template<typename Buffer>
stored_frame<Buffer>& stored_picture<Buffer>::frame() {
  switch(pt) {
  case picture_type::top: return *utils::container_of(static_cast<stored_field<Buffer>*>(this), &stored_frame<Buffer>::top);
  case picture_type::bot: return *utils::container_of(static_cast<stored_field<Buffer>*>(this), &stored_frame<Buffer>::bot);
  default: return *static_cast<stored_frame<Buffer>*>(this);
  }
}

template<typename Buffer>
stored_frame<Buffer> const& stored_picture<Buffer>::frame() const {
  return const_cast<stored_picture<Buffer>*>(this)->frame();
}

template<typename Buffer>
poc_t stored_picture<Buffer>::poc() const {
  switch(pt) {
  case picture_type::top: return {frame().top.poc, frame().top.poc};
  case picture_type::bot: return {frame().bot.poc, frame().bot.poc};
  default: return {frame().top.poc, frame().bot.poc};
  }
}

template<typename Buffer>
ref_type stored_picture<Buffer>::rt() const {
  switch(pt) {
  case picture_type::top: return frame().top.rt;
  case picture_type::bot: return frame().bot.rt;
  default:
    if(!frame().top.valid || !frame().bot.valid) return ref_type::none;
    if(frame().top.rt == ref_type::long_term && frame().bot.rt == ref_type::long_term)
      return ref_type::long_term;
    if(frame().top.rt == ref_type::short_term && frame().bot.rt == ref_type::short_term)
      return ref_type::short_term;
    return ref_type::none;
  }
}

template<typename Buffer>
void stored_picture<Buffer>::rt(ref_type rt) {
  if(pt == picture_type::top)
    frame().top.rt = rt;
  else if(pt == picture_type::bot)
    frame().bot.rt = rt;
  else
    frame().top.rt = frame().bot.rt = rt;
}

template<typename Buffer>
Buffer const& stored_picture<Buffer>::buffer() const {
  return frame().buffer;
}

template<typename Buffer>
bool is_short_term_reference(stored_picture<Buffer> const& p) { return p.rt() == ref_type::short_term; }

template<typename Buffer>
bool is_long_term_reference(stored_picture<Buffer> const& p) { return p.rt() == ref_type::long_term; }

template<typename Buffer>
bool is_reference(stored_picture<Buffer> const& p) { return p.rt() != ref_type::none; }

template<typename Buffer>
unsigned FrameNum(stored_picture<Buffer> const& p) {
  return p.frame().frame_num;
}

template<typename Buffer>
unsigned LongTermFrameIdx(stored_picture<Buffer> const& p) {
  return p.frame().long_term_frame_idx;
}

template<typename Buffer>
unsigned PicOrderCnt(stored_picture<Buffer> const& p) {
  switch(p.pt) {
  case picture_type::top: return p.poc().top;
  case picture_type::top: return p.poc().bot;
  default: return std::min(p.poc().top, p.poc().bot);
  }
}

void mark_as_unused_for_reference(std::uint32_t) {}

template<typename Buffer>
void mark_as_unused_for_reference(stored_picture<Buffer>& p) {
  if(has_top(p.pt)) p.frame().top.rt = ref_type::none;
  if(has_bot(p.pt)) p.frame().bot.rt = ref_type::none; 

  if(p.frame().rt() == ref_type::none)
    mark_as_unused_for_reference(p.buffer());
}

template<typename Buffer>
struct dpb_iterator : std::iterator<std::forward_iterator_tag, stored_picture<Buffer>> {
  stored_picture<Buffer>* p;

  dpb_iterator() = default;
  dpb_iterator(stored_picture<Buffer>* p) : p(p) {}

  stored_picture<Buffer>& operator*() const { return *p; }
  stored_picture<Buffer>* operator->() const { return p; }

  dpb_iterator& operator ++ () {
    switch(p->pt) {
    case picture_type::top: 
      p = &p->frame().bot;
      break;
    case picture_type::bot:
      p = &(&p->frame()+1)->top;
      break;
    default:
      p = &p->frame()+1;
      break;
    }
    return *this;
  }

  dpb_iterator operator ++ (int) {
    auto t = *this;
    ++(*this);
    return t;
  }

  friend bool operator == (dpb_iterator const& a, dpb_iterator const& b) { return a.p == b.p; }
  friend bool operator != (dpb_iterator const& a, dpb_iterator const& b) { return !(a==b); }
};

template<typename Buffer>
utils::range<dpb_iterator<Buffer>> field_view(std::vector<stored_frame<Buffer>>& dpb) {
  return {{&dpb.begin()->top}, {&dpb.end()->top}};
}

template<typename Buffer>
utils::range<dpb_iterator<Buffer>> frame_view(std::vector<stored_frame<Buffer>>& dpb) {
  return {dpb_iterator<Buffer>{&*dpb.begin()}, dpb_iterator<Buffer>{&*dpb.end()}};
}

template<typename Buffer>
struct picture {
  picture(picture_type pt = picture_type::frame) : pt(pt) {}

  picture(picture const&) = default;
  picture(picture&&) = default;

  picture& operator=(picture const&) = default;
  picture& operator=(picture&&) = default;

  picture(stored_picture<Buffer> const& p) : 
    pt(p.pt), rt(p.rt()), buffer(p.buffer()), poc(p.poc()), frame_num(p.frame().frame_num), long_term_frame_idx(p.frame().long_term_frame_idx) 
  {}

  picture_type pt;
  ref_type rt = ref_type::none;

  Buffer buffer;

  poc_t poc;

  unsigned frame_num;
  unsigned long_term_frame_idx;

  friend
  bool operator==(picture const& a, picture const& b) {
    return a.pt == b.pt && a.rt == b.rt && a.buffer == b.buffer;
  }

  friend bool is_reference(picture const& p) { return p.rt != ref_type::none; }

  friend bool is_short_term_reference(picture const& p) { return p.rt == ref_type::short_term; }

  friend bool is_long_term_reference(picture const& p) { return p.rt == ref_type::long_term; }

  friend unsigned LongTermFrameIdx(picture const& p) { 
    assert(p.rt == ref_type::long_term);
    return p.long_term_frame_idx;
  }

  friend unsigned FrameNum(picture const& p) {
    return p.frame_num;
  }

  friend unsigned PicOrderCnt(picture const& p) {
    switch(p.pt) {
    case picture_type::top: return p.poc.top;
    case picture_type::bot: return p.poc.bot;
    default: return std::min(p.poc.top, p.poc.bot);
    }
  }

  friend void mark_as_short_term_reference(picture& p) { p.rt = ref_type::short_term; }
};

template<typename Buffer>
void mark_as_long_term_reference(picture<Buffer>& p, unsigned idx = 0) {
  p.rt = ref_type::long_term;
  p.long_term_frame_idx = idx;
}

template<typename Buffer>
struct context : picture_parameter_set, slice_header {
  std::vector<stored_frame<Buffer>> dpb;

  using picture = h264::picture<Buffer>;
  using stored_picture = h264::stored_picture<Buffer>; 

  utils::optional<picture> curr_pic; 

  union {
    struct {
      unsigned prevPicOrderCntMsb;
      unsigned prevPicOrderCntLsb;
    }; 

    struct {
      unsigned prevFrameNum;
      unsigned prevFrameNumOffset;
    };
  };

  unsigned MaxFrameNum() const { 
    return 1 << (log2_max_frame_num_minus4 + 4);
  }

  unsigned MaxPicOrderCntLsb() const {
    return 1 << (log2_max_pic_order_cnt_lsb_minus4 + 4);
  }

  int FrameNumWrap(picture const& p) const {
    return FrameNum(p) > frame_num ? FrameNum(p) - MaxFrameNum() : FrameNum(p);
  }

  int PicNum(picture const& p) const {
    return pic_type == picture_type::frame ? FrameNumWrap(p) : 2 * FrameNumWrap(p) + (pic_type == p.pt);   
  }

  int CurrPicNum() const {
    return pic_type == picture_type::frame ? frame_num : frame_num * 2 + 1;
  }

  unsigned LongTermPicNum(picture const& p) const {
    return pic_type == picture_type::frame ? LongTermFrameIdx(p) : 2 * LongTermFrameIdx(p) + (pic_type == p.pt);
  }

  unsigned MaxPicNum() const {
    return pic_type == picture_type::frame ? MaxFrameNum() : MaxFrameNum()*2;
  }

  void mark_as_long_term_reference(dpb_iterator<Buffer> const& i, unsigned idx) {
    for(auto& a: field_view(dpb))
      if(is_long_term_reference(a) && LongTermFrameIdx(a) == idx && (&i->frame() != &a.frame()))
        mark_as_unused_for_reference(a);
    
    i->frame().long_term_frame_idx = idx;
    i->rt(ref_type::long_term);
  }

  bool has_mmco5() const { 
    for(auto& a: mmcos)
      if(a.id == 5) return true;
    return false;
  }

  unsigned ExpectedDeltaPerPicOrderCntCycle() const {
    return std::accumulate(offset_for_ref_frame.begin(), offset_for_ref_frame.end(), 0);
  }

  poc_t poc_cnt_type0() {
    if(IdrPicFlag) {
      prevPicOrderCntLsb = 0;
      prevPicOrderCntMsb = 0;
    }

    std::int32_t PicOrderCntMsb = prevPicOrderCntMsb;
   
    if(pic_order_cnt_lsb < prevPicOrderCntLsb && ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (MaxPicOrderCntLsb()/2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb();
    else if((pic_order_cnt_lsb > prevPicOrderCntLsb) && ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb()/2)))
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb();

    poc_t poc;
    if(pic_type != picture_type::bot)
      poc.top = PicOrderCntMsb + pic_order_cnt_lsb;
    
    if(pic_type == picture_type::frame)
      poc.bot = poc.top + delta_pic_order_cnt_bottom;
    else if(pic_type == picture_type::bot)
      poc.bot = PicOrderCntMsb + pic_order_cnt_lsb;

    if(nal_ref_idc) {
      if(has_mmco5()) {
        prevPicOrderCntMsb = 0;
        prevPicOrderCntLsb = (pic_type != picture_type::bot) ? poc.top : 0;
      }
      else {
        prevPicOrderCntMsb = PicOrderCntMsb;
        prevPicOrderCntLsb = pic_order_cnt_lsb;
      }
    }

    return poc;
  }

  poc_t poc_cnt_type1() {
    unsigned FrameNumOffset;

    if(IdrPicFlag)
      FrameNumOffset = 0;
    else if(prevFrameNum > frame_num)
      FrameNumOffset = prevFrameNumOffset + MaxFrameNum();
    else
      FrameNumOffset = prevFrameNumOffset;

    prevFrameNum = has_mmco5() ? 0 : frame_num;
    prevFrameNumOffset = has_mmco5() ? 0 : FrameNumOffset;

    auto num_ref_frames_in_pic_order_cnt_cycle = offset_for_ref_frame.size();
    unsigned absFrameNum = 0;
    if(num_ref_frames_in_pic_order_cnt_cycle != 0) absFrameNum = FrameNumOffset + frame_num;

    if(nal_ref_idc == 0 && absFrameNum > 0) absFrameNum -= 1;

    unsigned picOrderCntCycleCnt = 0;
    unsigned frameNumInPicOrderCntCycle = 0;

    if(absFrameNum > 0) {
      picOrderCntCycleCnt = (absFrameNum - 1) /  num_ref_frames_in_pic_order_cnt_cycle;
      frameNumInPicOrderCntCycle = (absFrameNum - 1) % num_ref_frames_in_pic_order_cnt_cycle;
    }

    std::int32_t  expectedPicOrderCnt = 0;
    if(absFrameNum > 0) {
      expectedPicOrderCnt = picOrderCntCycleCnt * ExpectedDeltaPerPicOrderCntCycle();
      for(size_t i = 0; i <= frameNumInPicOrderCntCycle; ++i)
        expectedPicOrderCnt += offset_for_ref_frame[i];
    }

    if(nal_ref_idc == 0) expectedPicOrderCnt += offset_for_non_ref_pic;
  
    if(pic_type == picture_type::frame) {
      auto top = expectedPicOrderCnt + delta_pic_order_cnt[0];
      return {top, top + offset_for_top_to_bottom_field + delta_pic_order_cnt[1]}; 
    }
    else if(pic_type == picture_type::top) {
      return {expectedPicOrderCnt + delta_pic_order_cnt[0], 0};
    }
    else {
      return {0, expectedPicOrderCnt + offset_for_top_to_bottom_field + delta_pic_order_cnt[0]};
    }
  } 

  poc_t poc_cnt_type2() {
    unsigned FrameNumOffset;

    if(IdrPicFlag)
      FrameNumOffset = 0;
    else if(prevFrameNum > frame_num)
      FrameNumOffset = prevFrameNumOffset + MaxFrameNum();
    else
      FrameNumOffset = prevFrameNumOffset;

    prevFrameNum = has_mmco5() ? 0 : frame_num;
    prevFrameNumOffset = has_mmco5() ? 0 : FrameNumOffset;

    int tempPicOrderCnt =  2 * (FrameNumOffset + frame_num);
    if(nal_ref_idc == 0) tempPicOrderCnt -= 1;

    if(pic_type == picture_type::frame)
      return {tempPicOrderCnt, tempPicOrderCnt};
    else if(pic_type == picture_type::top)
      return {tempPicOrderCnt, 0};
    return {0, tempPicOrderCnt};
  }

  poc_t decode_poc() {
    switch(pic_order_cnt_type) {
    case 0: return poc_cnt_type0();
    case 1: return poc_cnt_type1();
    case 2: return poc_cnt_type2();
    }
    
    throw std::logic_error("unsupported pic_order_cnt_type");
  }

  std::array<std::vector<picture>, 2> construct_reflists() {
    std::vector<picture> short_term_l0;
    std::vector<picture> short_term_l1;
    std::vector<picture> long_term_l;

    auto dpb_view = pic_type == picture_type::frame ? frame_view(dpb) : field_view(dpb);
    for(auto& a: dpb_view) {
      if(is_short_term_reference(a)) short_term_l0.push_back(a);
      if(is_long_term_reference(a)) long_term_l.push_back(a);
    }

    std::sort(long_term_l.begin(), long_term_l.end(), [](picture& a, picture& b) { return LongTermFrameIdx(a) < LongTermFrameIdx(b); });

    if(slice_type == coding_type::P) {
      std::sort(short_term_l0.begin(), short_term_l0.end(), [&](picture& a, picture& b) { return FrameNumWrap(a) > FrameNumWrap(b); });
    }
    else if(slice_type == coding_type::B) {
      short_term_l1 = short_term_l0;

      std::sort(short_term_l0.begin(), short_term_l0.end(), [&](picture& a, picture& b) {
        return ((PicOrderCnt(a) <= PicOrderCnt(*curr_pic)) ? std::make_tuple(false, -PicOrderCnt(a)-1) : std::make_tuple(true, PicOrderCnt(a)+1)) < 
               ((PicOrderCnt(b) <= PicOrderCnt(*curr_pic)) ? std::make_tuple(false, -PicOrderCnt(b)-1) : std::make_tuple(true, PicOrderCnt(b)+1));
      });

      std::sort(short_term_l1.begin(), short_term_l1.end(), [&](picture& a, picture& b) {
        return ((PicOrderCnt(a) > PicOrderCnt(*curr_pic)) ? std::make_tuple(false, PicOrderCnt(a)+1) : std::make_tuple(true, -PicOrderCnt(a)-1)) < 
               ((PicOrderCnt(b) > PicOrderCnt(*curr_pic)) ? std::make_tuple(false, PicOrderCnt(b)+1) : std::make_tuple(true, -PicOrderCnt(b)-1));
      });
    }

    std::array<std::vector<picture>, 2> reflists;

    // section 8.2.4.2.5
    if(pic_type != picture_type::frame) {
      utils::stable_alternate(short_term_l0.begin(), short_term_l0.end(), [&](picture& p) { return p.pt == pic_type; });
      utils::stable_alternate(short_term_l1.begin(), short_term_l1.end(), [&](picture& p) { return p.pt == pic_type; });
      utils::stable_alternate(long_term_l.begin(), long_term_l.end(), [&](picture& p) { return p.pt == pic_type; });
    }

    reflists[0] = std::move(short_term_l0);
    reflists[0].insert(reflists[0].end(), long_term_l.begin(), long_term_l.end());
    
    if(slice_type == coding_type::B) {
      reflists[1] = std::move(short_term_l1);
      reflists[1].insert(reflists[1].end(), long_term_l.begin(), long_term_l.end());
    }

    if(reflists[1].size() > 1 && reflists[0] == reflists[1]) std::swap(reflists[1][0], reflists[1][1]);
  
    // 8.2.4.3 Modification process for reference picture lists
    auto ref_pic_list_modification_lx = [&](std::vector<picture>& reflist, std::vector<std::pair<unsigned, unsigned>> const& ops) {
      auto picNumLXPred = CurrPicNum();
      for(std::size_t i = 0; i < reflist.size() && i < ops.size(); ++i) {
        auto pos = reflist.end();
        
        if(ops[i].first == 0 || ops[i].first == 1) {
          picNumLXPred += ((ops[i].first == 0) ? -(ops[i].second + 1) : (ops[i].second + 1));
          if(picNumLXPred < 0) picNumLXPred += MaxPicNum();
          if(picNumLXPred >= MaxPicNum()) picNumLXPred -= MaxPicNum();

          auto picNumLX = picNumLXPred > CurrPicNum() ? picNumLXPred - CurrPicNum() : picNumLXPred;

          pos = std::find_if(reflist.begin(), reflist.end(), [&](picture& p) { return !is_long_term_reference(p) && PicNum(p) == picNumLX; });    
        }
        else if(ops[i].first == 2) {
          pos = std::find_if(reflist.begin(), reflist.end(), [&](picture& p) { return is_long_term_reference(p) && LongTermPicNum(p) == ops[i].second; });
        }

        if(pos == reflist.end()) throw std::runtime_error("refpic_list_modification - picnum not found in reflist");
        
        if(pos - reflist.begin() >= i) {
          std::rotate(reflist.begin() + i, pos, pos + 1);
          //auto t = std::move(*pos);
          //reflist.erase(pos);
          //reflist.insert(reflist.begin() + i, std::move(t)); 
        }
        else
          reflist.insert(reflist.begin() + i, *pos);
      } 
    }; 
  
    ref_pic_list_modification_lx(reflists[0], ref_pic_list_modification[0]);
    ref_pic_list_modification_lx(reflists[1], ref_pic_list_modification[1]);
  
    reflists[0].resize(num_ref_idx_l0_active_minus1 + 1);
    if(slice_type == coding_type::B) 
      reflists[1].resize(num_ref_idx_l1_active_minus1 + 1);
 
    return reflists;
  }

  void dec_ref_pic_marking(dpb_iterator<Buffer> curr_pic) {
    // marking of current picture is in complete_picture function 
    if(!mmcos.empty()) 
      adaptive_ref_pic_marking(curr_pic);
    else 
      apply_sliding_window(); 
  }

  void adaptive_ref_pic_marking(dpb_iterator<Buffer> curr_pic) {
    auto view = pic_type == picture_type::frame ? frame_view(dpb) : field_view(dpb);
    auto begin = view.begin();
    auto end = view.end();

    for(auto& op: mmcos) {
      if(op.id == 1) {
        auto picnum = CurrPicNum() - (op.difference_of_pic_nums_minus1 + 1);
        auto i = std::find_if(begin, end, [&](stored_picture& a) { return is_short_term_reference(a) && PicNum(a) == picnum; });
        if(i != end)
          mark_as_unused_for_reference(*i);
        else
          throw std::runtime_error("dec_ref_pic_marking: picnum not found");
      }
      else if(op.id == 2) {
        auto i = std::find_if(begin, end, [&](stored_picture& a) { return is_long_term_reference(a) && LongTermPicNum(a) == op.long_term_pic_num; });
        if(i != end)
          mark_as_unused_for_reference(*i);
        else
          throw std::runtime_error("dec_ref_pic_marking: picnum not found");  
      }
      else if(op.id == 3) {
        auto picnum = CurrPicNum() - (op.difference_of_pic_nums_minus1 + 1);
        auto i = std::find_if(begin, end, [&](stored_picture& a) { return is_short_term_reference(a) && PicNum(a) == picnum; });
        if(i != end)
          mark_as_long_term_reference(i, op.long_term_frame_idx);
        else
          throw std::runtime_error("dec_ref_pic_marking: picnum not found");
      }
      else if(op.id == 4) {
        for(auto i = begin; i != end; ++i)
          if(is_long_term_reference(*i) && LongTermFrameIdx(*i) >= op.max_long_term_frame_idx_plus1)
            mark_as_unused_for_reference(*i);
      }
      else if(op.id == 5) {
        for(auto i = begin; i != end; ++i)
          if(i != curr_pic) mark_as_unused_for_reference(*i);
      }
      else if(op.id == 6) {
        mark_as_long_term_reference(curr_pic, op.long_term_frame_idx);
      }
    }
  }

  void apply_sliding_window() {
    int numShortTerm = 0, numLongTerm = 0;
    for(auto& a: dpb) {
      //if(is_short_term_reference(a.top) || is_short_term_reference(a.bot)) ++numShortTerm;
      //if(is_long_term_reference(a.top) || is_long_term_reference(a.bot)) ++numLongTerm;
      if(is_short_term_reference(a)) ++numShortTerm;
      if(is_long_term_reference(a)) ++numLongTerm;
    }

    int n = numShortTerm + numLongTerm - std::max(max_num_ref_frames, 1u);
    n = std::min(n, static_cast<int>(numShortTerm)); 
 
    while((n--)> 0)
      mark_as_unused_for_reference(*std::min_element(dpb.begin(), dpb.end(),
        [&](stored_frame<Buffer> const& a, stored_frame<Buffer> const& b) {
          return std::make_tuple(!is_short_term_reference(a), FrameNumWrap(a)) < std::make_tuple(!is_short_term_reference(b), FrameNumWrap(b));
        })); 
  }

  void new_picture() {
    if(curr_pic) complete_picture();

    curr_pic = picture{pic_type};
    curr_pic->poc = decode_poc();
    curr_pic->frame_num = frame_num;
    curr_pic->buffer = !dpb.empty() && is_complementary_pair() ? dpb.back().buffer : Buffer{};
  }

  bool is_complementary_pair() {
    auto& frame = dpb.back();
    return frame.top.valid && !frame.bot.valid 
      && curr_pic->pt == picture_type::bot 
      && curr_pic->frame_num == frame.frame_num 
      && !IdrPicFlag
      && !has_mmco5();
  }

  void complete_picture() {
    assert(curr_pic);

    if(has_mmco5()) {
      curr_pic->frame_num = 0;
      auto tmp = PicOrderCnt(*curr_pic);
      curr_pic->poc.top -= tmp;
      curr_pic->poc.bot -= tmp;
    } 

    if(nal_ref_idc) {
      mark_as_short_term_reference(*curr_pic); 
      if(IdrPicFlag) { 
        for(auto& a: dpb)
          mark_as_unused_for_reference(a);
  
        if(long_term_reference_flag) h264::mark_as_long_term_reference(*curr_pic, 0);
      }
    }

    if(!dpb.empty() && is_complementary_pair()) {
      dpb.back().bot.valid = true;
      dpb.back().bot.rt = curr_pic->rt;
      dpb.back().bot.poc = curr_pic->poc.bot;
    }
    else {
      stored_frame<Buffer> frame;
      frame.frame_num = curr_pic->frame_num;
      if(curr_pic->rt == ref_type::long_term) frame.long_term_frame_idx = curr_pic->long_term_frame_idx;
      frame.buffer = std::move(curr_pic->buffer);
      
      if(has_top(curr_pic->pt)) {
        frame.top.valid = true;
        frame.top.rt = curr_pic->rt;
        frame.top.poc = curr_pic->poc.top;
      }

      if(has_bot(curr_pic->pt)) {
        frame.bot.valid = true;
        frame.bot.rt = curr_pic->rt;
        frame.bot.poc = curr_pic->poc.bot;
      }
      dpb.push_back(std::move(frame));
    }
   
    curr_pic = utils::nullopt;

    dpb_iterator<Buffer> c;
    switch(pic_type) {
    case picture_type::frame: c = {&dpb.back()}; break;
    case picture_type::top: c = {&dpb.back().top}; break;
    case picture_type::bot: c = {&dpb.back().bot}; break;
    }
 
    if(nal_ref_idc) dec_ref_pic_marking(c); 
  }

  template<typename Source>
  bool process_slice_header(Source& a, std::vector<picture_parameter_set> const& ppss, unsigned nal_unit_type, unsigned nal_ref_idc) {
    slice_partial_header sph;

    parse_slice_header_up_to_pps_id(a, nal_unit_type, nal_ref_idc, sph);

    if(sph.pic_parameter_set_id != slice_header::pic_parameter_set_id) {
      if(curr_pic) complete_picture();
      if(ppss.size() <= sph.pic_parameter_set_id || !ppss[sph.pic_parameter_set_id].is_valid()) return false;
      static_cast<picture_parameter_set&>(*this) = ppss[sph.pic_parameter_set_id];
    }
    
    parse_slice_partial_header_after_pps_id(a, *this, sph);

    if(!curr_pic || is_new_picture(sph, *this)) {
      if(curr_pic) complete_picture();
      static_cast<slice_partial_header&>(*this) = sph;
    }
    else
      static_cast<slice_partial_header&>(*this) = sph;

    parse_slice_header_rest(a, *this, *this);

    if(!curr_pic) new_picture(); 
  
    return true;
  }
};

} // namespace h264

#endif
