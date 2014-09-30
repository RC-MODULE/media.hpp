#ifndef __h264_hpp__7a426e46_50b8_4b51_90be_d97461d1a39d__
#define __h264_hpp__7a426e46_50b8_4b51_90be_d97461d1a39d__

#include <array>
#include <vector>

#include "utils.hpp"
#include "bitstream.hpp"

namespace h264 {

enum class nalu_type {
  slice_layer_non_idr           = 1,
  slice_data_partition_a_layer  = 2,
  slice_data_partition_b_layer  = 3,
  slice_data_partition_c_layer  = 4,
  slice_layer_idr               = 5,
  sei                           = 6,
  seq_parameter_set             = 7,
  pic_parameter_set             = 8,
  access_unit_delimiter         = 9,
  end_of_seq                    = 10,
  end_of_stream                 = 11,
  filler_data                   = 12,
  seq_parameter_set_extension   = 13,
  prefix_nal_unit               = 14,
  subset_seq_parameter_set      = 15,
  slice_layer_aux               = 19,
  slice_layer_extension         = 20
};

enum class coding_type {
  P = 0, B = 1, I = 2, SP = 3, SI = 4
};

enum class picture_type { frame, top, bot };

inline bool has_top(picture_type p) { return p != picture_type::bot; }
inline bool has_bot(picture_type p) { return p != picture_type::top; }

inline picture_type opposite(picture_type pt) { return pt == picture_type::bot ? picture_type::top : picture_type::bot; }

struct nal_unit_header {
  std::uint8_t nal_ref_idc;
  std::uint8_t nal_unit_type;
};

template<typename Parser>
nal_unit_header parse_nal_unit_header(Parser& a) {
  nal_unit_header h;
  
  u(a, 1); // forbidden_zero_bit;
  h.nal_ref_idc = u(a, 2);
  h.nal_unit_type = u(a, 5);
  
  return h;
}

struct scaling_lists {
  std::array<std::array<std::uint8_t, 16>,6> lists_4x4;
  std::array<std::array<std::uint8_t, 64>,6> lists_8x8;
};

struct seq_parameter_set {
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

  utils::optional<scaling_lists> scaling_matrix; 

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

inline unsigned ChromaArrayType(seq_parameter_set const& sps) {
  return sps.separate_colour_plane_flag == 0 ? sps.chroma_format_idc : 0;
} 

inline unsigned MaxFrameNum(seq_parameter_set const& sps) { return 1 << (sps.log2_max_frame_num_minus4 + 4); }

struct pic_parameter_set {
  unsigned  pic_parameter_set_id = -1u;
  unsigned  seq_parameter_set_id;
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
  utils::optional<scaling_lists> scaling_matrix;
};

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

struct ref_pic_list_modification_operation {
  unsigned id;
  union {
    unsigned abs_diff_pic_num_minus1;
    unsigned long_term_pic_num;
  };
};

struct slice_header {
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

  unsigned redundant_pic_cnt;
  bool direct_spatial_mv_pred_flag = false;
  bool num_ref_idx_active_override_flag = false;
  unsigned num_ref_idx_l0_active_minus1;
  unsigned num_ref_idx_l1_active_minus1;

  std::vector<ref_pic_list_modification_operation> ref_pic_list_modification[2];

  unsigned luma_log2_weight_denom = 0;
  unsigned chroma_log2_weight_denom = 0;

  struct weight_pred_table_element {
    struct {
      std::int8_t weight;
      std::int8_t offset;
    } luma, cb, cr;
  };
  std::vector<weight_pred_table_element> weight_pred_table[2];

  bool no_output_of_prior_pics_flag = false;
  bool long_term_reference_flag = false;
  std::vector<memory_management_control_operation> mmcos;

  unsigned cabac_init_idc = 3; // msvd expects cabac_init_idc for i slices
  int slice_qp_delta = 0;

  unsigned disable_deblocking_filter_idc = 0;
  int slice_alpha_c0_offset_div2 = 0;
  int slice_beta_offset_div2 = 0;
};

struct parsing_context {
  std::vector<utils::optional<seq_parameter_set>> sparams;
  std::vector<utils::optional<pic_parameter_set>> pparams;


  utils::optional<seq_parameter_set> const& sps(unsigned n) const {
    static const utils::optional<seq_parameter_set> dummy;
    if(n < sparams.size()) return sparams[n];
    return dummy;
  }
  utils::optional<pic_parameter_set> const& pps(unsigned n) const {
    static const utils::optional<pic_parameter_set> dummy;
    if(n < pparams.size()) return pparams[n];
    return dummy;
  }
  utils::optional<seq_parameter_set> const& sps_by_pps_id(unsigned n) const {
    auto s = pps(n);
    if(s) return sps(s->seq_parameter_set_id);
    return sps(-1u);
  }
  utils::optional<seq_parameter_set> const& sps(slice_header const& s) const {
    return sps_by_pps_id(s.pic_parameter_set_id);
  }
  utils::optional<pic_parameter_set> const& pps(slice_header const& s) const {
    return pps(s.pic_parameter_set_id);
  }

  friend void add(parsing_context& cx, seq_parameter_set v) {
    if(cx.sparams.size() <= v.seq_parameter_set_id) cx.sparams.resize(v.seq_parameter_set_id+1);
    cx.sparams[v.seq_parameter_set_id] = v;
  }
  friend void add(parsing_context& cx, utils::optional<seq_parameter_set> v) { if(v) return add(cx, *v); }

  friend void add(parsing_context& cx, pic_parameter_set v) {
    if(cx.pparams.size() <= v.pic_parameter_set_id) cx.pparams.resize(v.pic_parameter_set_id+1);
    cx.pparams[v.pic_parameter_set_id] = v;
  }
  friend void add(parsing_context& cx, utils::optional<pic_parameter_set> v) { if(v) return add(cx, *v); }
};

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

template<typename Parser>
seq_parameter_set parse_sps(Parser& a) {
  seq_parameter_set sps;
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
    auto seq_scaling_matrix_present_flag = u(a, 1);
    if(seq_scaling_matrix_present_flag) {
      sps.scaling_matrix = scaling_lists{};
      parse_scaling_lists(a, sps.chroma_format_idc, default_scaling_lists_4x4, default_scaling_lists_8x8, sps.scaling_matrix->lists_4x4, sps.scaling_matrix->lists_8x8);
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

template<typename Parser>
utils::optional<pic_parameter_set> parse_pps(parsing_context const& cx, Parser& a) {
  pic_parameter_set pps;

  pps.pic_parameter_set_id = ue(a);
  pps.seq_parameter_set_id = ue(a);

  auto& sps = cx.sps(pps.seq_parameter_set_id);
  if(!sps) return utils::nullopt;

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
    auto pic_scaling_matrix_present_flag = u(a,1);
    if(pic_scaling_matrix_present_flag) {
      pps.scaling_matrix = scaling_lists{};
      if(sps->scaling_matrix)
        parse_scaling_lists(a, sps->chroma_format_idc, sps->scaling_matrix->lists_4x4, sps->scaling_matrix->lists_8x8, pps.scaling_matrix->lists_4x4, pps.scaling_matrix->lists_8x8);
      else
        parse_scaling_lists(a, sps->chroma_format_idc, default_scaling_lists_4x4, default_scaling_lists_8x8, pps.scaling_matrix->lists_4x4, pps.scaling_matrix->lists_8x8);
    }
    pps.second_chroma_qp_index_offset = se(a);
  }

  return pps;
}

template<typename Parser>
utils::optional<slice_header> parse_slice_header(parsing_context const& cx, Parser& a, unsigned nal_unit_type, unsigned nal_ref_idc) {
  slice_header slice;

  slice.IdrPicFlag = nal_unit_type == 5;
  slice.nal_ref_idc = nal_ref_idc;

  slice.first_mb_in_slice = ue(a);
  slice.slice_type = static_cast<coding_type>(ue(a) % 5);
  slice.pic_parameter_set_id = ue(a);

  auto& pps = cx.pps(slice.pic_parameter_set_id);
  if(!pps) return utils::nullopt;

  auto& sps = *cx.sps(pps->seq_parameter_set_id); 

  if(sps.separate_colour_plane_flag)
    slice.colour_plane_id = ue(a);

  slice.frame_num = u(a, sps.log2_max_frame_num_minus4+4);
 
  slice.pic_type = picture_type::frame;
  if(!sps.frame_mbs_only_flag) {
    if(u(a,1))
      slice.pic_type = u(a,1) ? picture_type::bot : picture_type::top;
  }

  if(slice.IdrPicFlag)
    slice.idr_pic_id = ue(a);

  if(sps.pic_order_cnt_type == 0) {
    slice.pic_order_cnt_lsb = u(a, sps.log2_max_pic_order_cnt_lsb_minus4 + 4);
    slice.delta_pic_order_cnt_bottom = 0;
    if(pps->bottom_field_pic_order_in_frame_present_flag && slice.pic_type == picture_type::frame)
      slice.delta_pic_order_cnt_bottom = se(a);
  }
  else if(sps.pic_order_cnt_type == 1 && !sps.delta_pic_order_always_zero_flag) {
    slice.delta_pic_order_cnt[0] = se(a);
    slice.delta_pic_order_cnt[1] = (pps->bottom_field_pic_order_in_frame_present_flag && slice.pic_type == picture_type::frame) ? se(a) : 0;
  }
  else {
    slice.delta_pic_order_cnt[0] = slice.delta_pic_order_cnt[1] = 0;
  }

  slice.direct_spatial_mv_pred_flag =  slice.slice_type == coding_type::B ? u(a, 1) : false;

  if(slice.slice_type == coding_type::P || slice.slice_type == coding_type::B) {
    slice.num_ref_idx_l0_active_minus1 = pps->num_ref_idx_l0_default_active_minus1;
    slice.num_ref_idx_l1_active_minus1 = pps->num_ref_idx_l1_default_active_minus1;
    slice.num_ref_idx_active_override_flag = u(a, 1);
    if(slice.num_ref_idx_active_override_flag) {
      slice.num_ref_idx_l0_active_minus1 = ue(a);
      if(slice.slice_type == coding_type::B)
        slice.num_ref_idx_l1_active_minus1 = ue(a);
    }
  }

  auto ref_pic_list_modification = [&](std::vector<ref_pic_list_modification_operation>& ops) {
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

  if((pps->weighted_pred_flag && slice.slice_type == coding_type::P) || (pps->weighted_bipred_idc == 1 && slice.slice_type == coding_type::B)) {
    slice.luma_log2_weight_denom = ue(a);
    if(ChromaArrayType(sps) != 0)
      slice.chroma_log2_weight_denom = ue(a);

    auto read_weight_pred_table = [&](int n, decltype(slice.weight_pred_table[0])& wt) {
      wt.resize(n);
      
      for(int i = 0; i != n; ++i) {
        wt[i].luma.weight = 1 << slice.luma_log2_weight_denom;
        wt[i].luma.offset = 0;

        if(u(a,1)) {
          wt[i].luma.weight = se(a);
          wt[i].luma.offset = se(a);
        }
        
        if(ChromaArrayType(sps) != 0) {
          wt[i].cb.weight = wt[i].cr.weight = 1 << slice.chroma_log2_weight_denom;
          wt[i].cb.offset = wt[i].cr.offset = 0;
  
          if(u(a,1)) {
            wt[i].cb.weight = se(a);
            wt[i].cb.offset = se(a);
            wt[i].cr.weight = se(a);
            wt[i].cr.offset = se(a);
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
  if(pps->entropy_coding_mode_flag && slice.slice_type != coding_type::I)
    slice.cabac_init_idc = ue(a);

  slice.slice_qp_delta = se(a);

  if(pps->deblocking_filter_control_present_flag) {
    slice.disable_deblocking_filter_idc = ue(a);
    if(slice.disable_deblocking_filter_idc != 1) {
      slice.slice_alpha_c0_offset_div2 = se(a);
      slice.slice_alpha_c0_offset_div2 = se(a);
    }
  }
  return slice;
}

inline bool has_mmco5(slice_header const& s) {
  return std::any_of(s.mmcos.begin(), s.mmcos.end(), [](memory_management_control_operation const& o) { return o.id == 5; });
}

}

#endif
