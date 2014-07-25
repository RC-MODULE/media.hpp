#ifndef __H264_HPP__
#define __H264_HPP__

#include <vector>
#include <stdexcept>
#include <iterator>
#include <algorithm>
#include "bitstream.hpp"
#include "utils.hpp" 

namespace H264 {

using utils::variant;

enum class PicType { Frame, Top, Bottom};

inline bool has_top(PicType pt) { return pt == PicType::Frame || pt == PicType::Top; }
inline bool has_bottom(PicType pt) { return pt == PicType::Frame || pt == PicType::Bottom; }

enum class SliceType { P, B, I };

inline
SliceType cast_to_slice_type(std::uint32_t t) {
  switch(t % 5) {
  case 0: return SliceType::P;
  case 1: return SliceType::B;
  case 2: return SliceType::I;
  default: throw std::runtime_error("Unsupported slice type");
  }
}

struct SequenceParameterSet {
  unsigned  profile_idc;
  bool      constrained_set0_flag = false;
  bool      constrained_set1_flag = false;
  bool      constrained_set2_flag = false;
  bool      constrained_set3_flag = false;
  bool      constrained_set4_flag = false;
  bool      constrained_set5_flag = false;
  unsigned  level_idc;
  unsigned  seq_parameter_set_id;
  
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
std::uint16_t pic_width_in_mbs(SequenceParameterSet const& sps) { return sps.pic_width_in_mbs_minus1 + 1; }
inline
std::uint16_t pic_height_in_mbs(SequenceParameterSet const& sps) { return (sps.pic_height_in_map_units_minus1+1)*(sps.frame_mbs_only_flag ? 1 : 2); } 

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
SequenceParameterSet parse_sps(Source& a) {
  SequenceParameterSet sps;
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

inline
std::vector<SequenceParameterSet>& add(std::vector<SequenceParameterSet>& spss, SequenceParameterSet&& sps) {
  spss.resize(std::max(spss.size(), std::size_t(sps.seq_parameter_set_id + 1)));
  spss[sps.seq_parameter_set_id] = sps;
  return spss;
}

struct PictureParameterSet : SequenceParameterSet {
  unsigned  pic_parameter_set_id;
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
};

template<typename Source>
PictureParameterSet parse_pps(Source& a, std::vector<SequenceParameterSet> const& spss) {
  PictureParameterSet pps;

  pps.pic_parameter_set_id = ue(a);
  pps.seq_parameter_set_id = ue(a);
  static_cast<SequenceParameterSet&>(pps) = spss.at(pps.seq_parameter_set_id);

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

inline
std::vector<PictureParameterSet>& add(std::vector<PictureParameterSet>& ppss, PictureParameterSet&& pps) {
  ppss.resize(std::max(ppss.size(), std::size_t(pps.pic_parameter_set_id + 1)));
  ppss[pps.pic_parameter_set_id] = pps;
  return ppss;
}

inline unsigned MaxFrameNum(PictureParameterSet const& pps) { return 1 << (pps.log2_max_frame_num_minus4 + 4); }
inline unsigned MaxPicOrderCntLsb(PictureParameterSet const& pps) { return 1 << (pps.log2_max_pic_order_cnt_lsb_minus4 + 4);}
inline unsigned ChromaArrayType(PictureParameterSet const& pps) { return pps.separate_colour_plane_flag == 0 ? pps.chroma_format_idc : 0; }

struct MemoryManagementControlOperation {
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

struct SliceHeader {
  bool IdrPicFlag;
  unsigned nal_ref_idc;

  unsigned first_mb_in_slice;
  SliceType slice_type;
  unsigned pic_parameter_set_id;
  PicType pic_type;
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

  std::vector<std::pair<unsigned, unsigned>> ref_pic_list_modification[2];

  unsigned luma_log2_weight_denom = 0;
  unsigned chroma_log2_weight_denom = 0;

  struct WeightPredTableElement {
    std::int8_t weight;
    std::int8_t offset;
  };
  struct ChromaWeightPredTableElement {
    WeightPredTableElement cb;
    WeightPredTableElement cr;
  };
  struct {
    std::vector<WeightPredTableElement> luma;
    std::vector<ChromaWeightPredTableElement> chroma;
  } weight_pred_table[2];

  bool no_output_of_prior_pics_flag = false;
  bool long_term_reference_flag = false;
  std::vector<MemoryManagementControlOperation> memory_management_control_operations;

  unsigned cabac_init_idc = 3; // msvd expects cabac_init_idc for i slices
  int slice_qp_delta = 0;

  unsigned disable_deblocking_filter_idc = 0;
  int slice_alpha_c0_offset_div2 = 0;
  int slice_beta_offset_div2 = 0;
};

template<typename Source>
inline
SliceHeader parse_slice(Source& a, std::vector<PictureParameterSet> const& ppss, unsigned nal_unit_type, unsigned nal_ref_idc) {
  SliceHeader slice;
  slice.IdrPicFlag = nal_unit_type == 5;
  slice.nal_ref_idc = nal_ref_idc;

  slice.first_mb_in_slice = ue(a);
  slice.slice_type = cast_to_slice_type(ue(a));
  slice.pic_parameter_set_id = ue(a);

  auto& pps = ppss.at(slice.pic_parameter_set_id);

  if(pps.separate_colour_plane_flag)
    slice.colour_plane_id = ue(a);

  slice.frame_num = u(a, pps.log2_max_frame_num_minus4+4);
  
  slice.pic_type = PicType::Frame;
  if(!pps.frame_mbs_only_flag) {
    if(u(a,1))
      slice.pic_type = u(a,1) ? PicType::Bottom : PicType::Top;
  }

  if(slice.IdrPicFlag)
    slice.idr_pic_id = ue(a);

  if(pps.pic_order_cnt_type == 0) {
    slice.pic_order_cnt_lsb = u(a, pps.log2_max_pic_order_cnt_lsb_minus4 + 4);
    slice.delta_pic_order_cnt_bottom = 0;
    if(pps.bottom_field_pic_order_in_frame_present_flag && slice.pic_type == PicType::Frame)
      slice.delta_pic_order_cnt_bottom = se(a);
  }
  else if(pps.pic_order_cnt_type == 1 && !pps.delta_pic_order_always_zero_flag) {
    slice.delta_pic_order_cnt[0] = se(a);
    slice.delta_pic_order_cnt[1] = (pps.bottom_field_pic_order_in_frame_present_flag && slice.pic_type == PicType::Frame) ?
      se(a) : 0;
  }
  else {
    slice.delta_pic_order_cnt[0] = slice.delta_pic_order_cnt[1] = 0;
  }

  slice.direct_spatial_mv_pred_flag =  slice.slice_type == SliceType::B ? u(a, 1) : false;

  if(slice.slice_type == SliceType::P || slice.slice_type == SliceType::B) {
    slice.num_ref_idx_l0_active_minus1 = pps.num_ref_idx_l0_default_active_minus1;
    slice.num_ref_idx_l1_active_minus1 = pps.num_ref_idx_l1_default_active_minus1;
    slice.num_ref_idx_active_override_flag = u(a, 1);
    if(slice.num_ref_idx_active_override_flag) {
      slice.num_ref_idx_l0_active_minus1 = ue(a);
      if(slice.slice_type == SliceType::B)
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

  if(slice.slice_type != SliceType::I)
    if(u(a,1)) //ref_pic_list_modification_flag_l0
      ref_pic_list_modification(slice.ref_pic_list_modification[0]);

  if(slice.slice_type == SliceType::B)
    if(u(a,1)) // ref_pic_list_modification_flag_l1
      ref_pic_list_modification(slice.ref_pic_list_modification[1]);
 
  if((pps.weighted_pred_flag && slice.slice_type == SliceType::P) || (pps.weighted_bipred_idc == 1 && slice.slice_type == SliceType::B)) {
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
    if(slice.slice_type == SliceType::B)
      read_weight_pred_table(slice.num_ref_idx_l1_active_minus1+1, slice.weight_pred_table[1]);
  }

  if(nal_ref_idc) {
    if(slice.IdrPicFlag) {
      slice.no_output_of_prior_pics_flag = u(a, 1);
      slice.long_term_reference_flag = u(a, 1);
    }
    else if(u(a,1)) {
      slice.memory_management_control_operations.clear();
      for(;;) {
        MemoryManagementControlOperation mmco;
        mmco.id = ue(a);
        if(mmco.id == 0) break;
        
        if(mmco.id == 1 || mmco.id == 3) mmco.difference_of_pic_nums_minus1 = ue(a);
        if(mmco.id == 2) mmco.long_term_pic_num = ue(a);
        if(mmco.id == 3 || mmco.id == 6) mmco.long_term_frame_idx = ue(a);
        if(mmco.id == 4) mmco.max_long_term_frame_idx_plus1 = ue(a);

        slice.memory_management_control_operations.push_back(mmco);
      }
    }
  }

  slice.cabac_init_idc = 3;
  if(pps.entropy_coding_mode_flag && slice.slice_type != SliceType::I)
    slice.cabac_init_idc = ue(a);

  slice.slice_qp_delta = se(a);

  if(pps.deblocking_filter_control_present_flag) {
    slice.disable_deblocking_filter_idc = ue(a);
    if(slice.disable_deblocking_filter_idc != 1) {
      slice.slice_alpha_c0_offset_div2 = se(a);
      slice.slice_alpha_c0_offset_div2 = se(a);
    }
  }
  return slice;
}

inline bool has_mmco5(SliceHeader const& a) {
  return std::any_of(a.memory_management_control_operations.begin(), a.memory_management_control_operations.end(),
    [](MemoryManagementControlOperation const& op) { return op.id == 5; });
}

// 7.4.1.2.4 Detection of the first VCL NAL unit of a primary coded picture
inline
bool is_new_picture(SliceHeader const& a, SliceHeader const& b) {
  return a.pic_type != b.pic_type
        || a.pic_parameter_set_id != b.pic_parameter_set_id
        || a.frame_num != b.frame_num
        || (a.nal_ref_idc != b.nal_ref_idc && (a.nal_ref_idc == 0 || b.nal_ref_idc == 0))
        || a.IdrPicFlag != b.IdrPicFlag
        || (a.IdrPicFlag && (a.idr_pic_id != b.idr_pic_id))
        || a.pic_order_cnt_lsb != b.pic_order_cnt_lsb || a.delta_pic_order_cnt_bottom != b.delta_pic_order_cnt_bottom;
}

struct POC  {
  POC() = default;
  POC(std::uint32_t top, std::uint32_t bottom) : top(top), bottom(bottom) {}

  std::uint32_t top = 0;
  std::uint32_t bottom = 0;
};

struct PocCntType0 {
  PocCntType0(std::uint32_t MaxPicOrderCntLsb) : MaxPicOrderCntLsb(MaxPicOrderCntLsb) {}

  POC operator()(PicType pic_type, bool is_reference, std::uint32_t pic_order_cnt_lsb, std::int32_t delta_pic_order_cnt_bottom, bool has_mmco5) {
    std::int32_t PicOrderCntMsb = prevPicOrderCntMsb;
   
    if(pic_order_cnt_lsb < prevPicOrderCntLsb && ((prevPicOrderCntLsb - pic_order_cnt_lsb) >= (MaxPicOrderCntLsb/2)))
      PicOrderCntMsb = prevPicOrderCntMsb + MaxPicOrderCntLsb;
    else if((pic_order_cnt_lsb > prevPicOrderCntLsb) && ((pic_order_cnt_lsb - prevPicOrderCntLsb) > (MaxPicOrderCntLsb / 2)))
      PicOrderCntMsb = prevPicOrderCntMsb - MaxPicOrderCntLsb;

    POC poc;
    if(pic_type != PicType::Bottom)
      poc.top = PicOrderCntMsb + pic_order_cnt_lsb;
    if(pic_type == PicType::Frame)
      poc.bottom = poc.top + delta_pic_order_cnt_bottom;
    else if(pic_type == PicType::Bottom)
      poc.bottom = PicOrderCntMsb + pic_order_cnt_lsb;

    if(is_reference) {
      if(has_mmco5) {
        prevPicOrderCntMsb = 0;
        prevPicOrderCntLsb = pic_type != PicType::Bottom ? poc.top : 0;
      }
      else {
        prevPicOrderCntMsb = PicOrderCntMsb;
        prevPicOrderCntLsb = pic_order_cnt_lsb;
      }
    }

    return poc;
  }
private:
  const std::uint32_t MaxPicOrderCntLsb;
  std::int32_t prevPicOrderCntMsb = 0;
  std::uint32_t prevPicOrderCntLsb = 0;
};

struct PocCntType1 {
  PocCntType1(std::uint32_t MaxFrameNum, std::vector<std::int32_t> offset_for_ref_frame, int offset_for_non_ref_pic, int offset_for_top_to_bottom_field) :
    MaxFrameNum(MaxFrameNum),
    ExpectedDeltaPerPicOrderCntCycle(calc_expected_delta(offset_for_ref_frame)),
    offset_for_ref_frame(std::move(offset_for_ref_frame)),
    offset_for_non_ref_pic(offset_for_non_ref_pic),
    offset_for_top_to_bottom_field(offset_for_top_to_bottom_field)
  {}

  POC operator()(PicType pic_type, bool is_reference, std::uint32_t frame_num, int const delta_pic_order_cnt[2], bool has_mmco5) {
    auto FrameNumOffset = prevFrameNumOffset;
    if(prevFrameNum > frame_num)
      FrameNumOffset += MaxFrameNum;

    prevFrameNum = has_mmco5 ? 0 : frame_num;
    prevFrameNumOffset = has_mmco5 ? 0 : FrameNumOffset;

    auto num_ref_frames_in_pic_order_cnt_cycle = offset_for_ref_frame.size();
    unsigned absFrameNum = 0;
    if(num_ref_frames_in_pic_order_cnt_cycle != 0) absFrameNum = FrameNumOffset + frame_num;

    if(!is_reference && absFrameNum > 0) absFrameNum -= 1;

    unsigned picOrderCntCycleCnt = 0;
    unsigned frameNumInPicOrderCntCycle = 0;

    if(absFrameNum > 0) {
      picOrderCntCycleCnt = (absFrameNum - 1) /  num_ref_frames_in_pic_order_cnt_cycle;
      frameNumInPicOrderCntCycle = (absFrameNum - 1) % num_ref_frames_in_pic_order_cnt_cycle;
    }

    std::int32_t  expectedPicOrderCnt = 0;
    if(absFrameNum > 0) {
      expectedPicOrderCnt = picOrderCntCycleCnt * ExpectedDeltaPerPicOrderCntCycle;
      for(size_t i = 0; i <= frameNumInPicOrderCntCycle; ++i)
        expectedPicOrderCnt += offset_for_ref_frame[i];
    }

    if(!is_reference) expectedPicOrderCnt += offset_for_non_ref_pic;

    POC poc;
    if(pic_type == PicType::Frame) {
      poc.top = expectedPicOrderCnt + delta_pic_order_cnt[0];
      poc.bottom = poc.top + offset_for_top_to_bottom_field + delta_pic_order_cnt[1];
    }
    else if(pic_type == PicType::Top) {
      poc.top = expectedPicOrderCnt + delta_pic_order_cnt[0];
      poc.bottom = 0;
    }
    else {
      poc.bottom = expectedPicOrderCnt + offset_for_top_to_bottom_field + delta_pic_order_cnt[0];
    }

    return poc;
  }
private:
  static std::int32_t calc_expected_delta(std::vector<std::int32_t> const& offset_for_ref_frame) {
    std::int32_t a = 0;
    for(auto x: offset_for_ref_frame)
      a += x;
    return a;
  }

  const std::uint32_t MaxFrameNum;
  const std::int32_t ExpectedDeltaPerPicOrderCntCycle;
  const std::vector<std::int32_t> offset_for_ref_frame;
  const std::int32_t offset_for_non_ref_pic;
  const std::int32_t offset_for_top_to_bottom_field;

  std::uint32_t prevFrameNum = 0;
  std::uint32_t prevFrameNumOffset = 0;
};

struct PocCntType2 {
  PocCntType2(std::uint32_t MaxFrameNum) : MaxFrameNum(MaxFrameNum) {}

  POC operator()(PicType pic_type, bool is_reference, std::uint32_t frame_num, bool has_mmco5) {
    auto FrameNumOffset = prevFrameNumOffset;
    if(prevFrameNum > frame_num)
      FrameNumOffset += MaxFrameNum;

    prevFrameNum = has_mmco5 ? 0 : frame_num;
    prevFrameNumOffset = has_mmco5 ? 0 : FrameNumOffset;

    auto tempPicOrderCnt =  2 * (FrameNumOffset + frame_num);
    if(!is_reference) tempPicOrderCnt -= 1;

    if(pic_type == PicType::Frame)
      return {tempPicOrderCnt, tempPicOrderCnt};
    else if(pic_type == PicType::Top)
      return POC{tempPicOrderCnt, 0};
    return POC{0, tempPicOrderCnt};
  }
private:
  const std::uint32_t MaxFrameNum;

  std::uint32_t prevFrameNum = 0;
  std::uint32_t prevFrameNumOffset = 0;
};

inline
Variant::variant<PocCntType0, PocCntType1, PocCntType2> init_poc(PictureParameterSet const& pps) {
  if(pps.pic_order_cnt_type == 0) return PocCntType0(MaxPicOrderCntLsb(pps));
  else if(pps.pic_order_cnt_type == 1) return PocCntType1(MaxFrameNum(pps), pps.offset_for_ref_frame, pps.offset_for_non_ref_pic, pps.offset_for_top_to_bottom_field);
  return PocCntType2(MaxFrameNum(pps));
}

inline
POC decode_poc(Variant::variant<PocCntType0, PocCntType1, PocCntType2>& cnt, SliceHeader const& slice) {
  if(cnt.tag() == 0)
    return cnt.get<0>()(slice.pic_type, slice.nal_ref_idc, slice.pic_order_cnt_lsb, slice.delta_pic_order_cnt_bottom, has_mmco5(slice)); 
  else if(cnt.tag() == 1)
    return cnt.get<1>()(slice.pic_type, slice.nal_ref_idc, slice.frame_num, slice.delta_pic_order_cnt, has_mmco5(slice));
  else 
    return cnt.get<2>()(slice.pic_type, slice.nal_ref_idc, slice.frame_num, has_mmco5(slice));
}

enum class RefType {
  None, Short, Long
};

template<typename Buffer> struct Picture;

template<typename Buffer>
struct Frame {
  Frame() {}
  Frame(Picture<Buffer>&&);

  PicType pt;
  
  struct Field {
    PicType pt;
    std::uint16_t frame_num = 0;
    std::uint16_t long_term_frame_idx = 0;
    std::uint32_t poc = 0;
    RefType ref = RefType::None;
    Buffer buffer;

    friend bool is_short_term_reference(Field const& f) { return f.ref == RefType::Short; }
    friend bool is_long_term_reference(Field const& f) { return f.ref == RefType::Long; }
    friend bool is_reference(Field const& f) { return is_short_term_reference(f) || is_long_term_reference(f); }
  
    friend std::uint16_t FrameNum(Field const& f) { return f.frame_num; }
    std::uint16_t& FrameNum(Field& f) { return f.frame_num; }
    friend std::uint16_t LongTermFrameIdx(Field const& f) { return f.long_term_frame_idx; }
    friend int PicOrderCnt(Field const& f) { return f.poc; }

    friend void mark_as_unused_for_reference(Field& f) { f.ref = RefType::None; }
    friend void mark_as_short_term_reference(Field& f) { f.ref = RefType::Short; }
    friend void mark_as_long_term_reference(Field& f, std::uint16_t idx) { 
      f.ref = RefType::Long;
      f.long_term_frame_idx = idx;
    }
  };

  Field top;
  Field bottom;
  bool output = false;

  friend bool is_short_term_reference(Frame const& f) {
    return f.pt == PicType::Frame && ((is_short_term_reference(f.top) && is_reference(f.bottom)) || (is_reference(f.top) && is_short_term_reference(f.bottom)));
  }
  friend bool is_long_term_reference(Frame const& f) {
    return f.pt == PicType::Frame && is_long_term_reference(f.top) && is_long_term_reference(f.bottom);
  }
  friend bool is_reference(Frame const& f) { return is_short_term_reference(f) || is_long_term_reference(f); }
  
  friend std::uint16_t FrameNum(Frame const& f) { return f.top.frame_num; }
  friend std::uint16_t& FrameNum(Frame& f) { return f.top.frame_num; }
  friend std::uint16_t LongTermFrameIdx(Frame const& f) { return f.bottom.long_term_frame_idx; }
  friend int PicOrderCnt(Frame const& f) { return f.pt == PicType::Frame ? std::min(f.top.poc, f.bottom.poc) : (f.pt == PicType::Top ? f.top.poc : f.bottom.poc); }

  friend void mark_as_unused_for_reference(Frame& f) { f.top.ref = (f.bottom.ref = RefType::None); }
  friend void mark_as_short_term_reference(Frame& f) { f.top.ref = f.bottom.ref = RefType::Short; }
  friend void mark_as_long_term_reference(Frame& f, std::uint16_t idx) {
    f.top.ref = f.bottom.ref = RefType::Long;
    f.top.long_term_frame_idx = f.bottom.long_term_frame_idx = idx;
  }
};

template<typename Buffer>
struct Picture {
  Picture() {}
  Picture(Frame<Buffer> const& f) : 
    pt(f.pt), frame_num(FrameNum(f)), long_term_frame_idx(LongTermFrameIdx(f)), poc{f.top.poc, f.bottom.poc}, ref(f.top.ref), buffer(f.top.buffer) {}
  
  Picture(typename Frame<Buffer>::Field const& f) :
    pt(f.pt), frame_num(FrameNum(f)), long_term_frame_idx(LongTermFrameIdx(f)), poc{f.poc, f.poc}, ref(f.ref), buffer(f.buffer) {}

  Picture(PicType pt, unsigned frame_num, POC const& poc, Buffer&& buffer) : pt(pt), frame_num(frame_num), poc(poc), buffer(buffer) {}

  friend unsigned FrameNum(Picture const& p) { return p.frame_num; }
  friend int LongTermFrameIdx(Picture const& p) { return p.long_term_frame_idx; }
  friend int PicOrderCnt(Picture const& p) { return p.pt == PicType::Frame ? std::min(p.poc.top, p.poc.bottom) : (p.pt == PicType::Top ? p.poc.top : p.poc.bottom); }
  friend bool operator==(Picture const& a, Picture const& b) { return a.pt == b.pt && a.poc.top == b.poc.top && a.poc.bottom == b.poc.bottom; }
  friend bool is_long_term_reference(Picture const& p) { return p.ref == RefType::Long; }

  PicType pt = PicType::Top;
  unsigned frame_num = 0;
  unsigned long_term_frame_idx = 0;
  POC poc;
  RefType ref = RefType::None;
  Buffer buffer;
};

template<typename Buffer>
bool is_complementary_pair(Frame<Buffer> const& top, SliceHeader const& b) {
  return top.pt == PicType::Top && b.pic_type == PicType::Bottom
        && top.top.frame_num == b.frame_num
        && !b.IdrPicFlag
        && !has_mmco5(b);
}

template<typename Buffer>
Frame<Buffer>& merge_complementary_pair(Frame<Buffer>& frame, Picture<Buffer>&& bot) {
  frame.bottom.pt = PicType::Bottom;
  frame.bottom.frame_num = frame.top.frame_num;
  frame.bottom.poc = bot.poc.bottom;
  frame.bottom.buffer = bot.buffer;
  frame.pt = PicType::Frame;
  return frame;
}

template<typename Buffer>
inline Frame<Buffer>::Frame(Picture<Buffer>&& p) : pt(p.pt) {
  top.pt = PicType::Top;
  bottom.pt = PicType::Bottom;
  top.frame_num = bottom.frame_num = p.frame_num;
  top.ref = bottom.ref = p.ref;
  top.poc = p.poc.top;
  bottom.poc = p.poc.bottom;
  if(pt == PicType::Frame) 
    bottom.buffer = (top.buffer = std::move(p.buffer));
  else if(pt == PicType::Top)
    top.buffer = std::move(p.buffer);
  else
    bottom.buffer = std::move(p.buffer);
}

template<typename I>
// I - ForwardIterator<Frame<Buffer>>
// iterates over fields in decoded pictures buffer
class FieldIterator : public std::iterator<std::forward_iterator_tag, decltype(std::declval<I>()->top)> { 
  PicType pt;
  I it;
  I end;
public:
  auto operator*() const -> decltype(*&(it->top)) { return pt == PicType::Bottom ? it->bottom : it->top; }
  auto operator->() const -> decltype(&(it->top)) { return &**this; }

  FieldIterator& operator++() {
    if(pt == PicType::Top && has_bottom(it->pt))
      pt = PicType::Bottom;
    else {
      ++it;
      pt = (it == end || has_top(it->pt)) ? PicType::Top : PicType::Bottom;
    }
    return *this;
  }

  friend bool operator == (FieldIterator const& a, FieldIterator const& b) { return a.pt == b.pt && a.it == b.it && a.end == b.end; }
  friend bool operator != (FieldIterator const& a, FieldIterator const& b) { return !(a==b); }
  friend bool is_same_frame(FieldIterator const& a, I const& b) { return a.it == b; }
  friend bool is_same_frame(FieldIterator const& a, FieldIterator const& b) { return a.it == b.it; }

  FieldIterator(I it, I end, PicType pt) : pt(pt), it(it), end(end) {}
};

template<typename I>
FieldIterator<I> field_iterator(I it, I end, PicType pt = PicType::Top) { return {it, end, pt}; }

template<typename I>
inline std::pair<FieldIterator<I>,FieldIterator<I>> field_view(I begin, I end) {
  return {field_iterator(begin, end, begin == end || has_top(begin->pt) ? PicType::Top : PicType::Bottom), field_iterator(end, end)};
}

template<typename I>
inline std::pair<FieldIterator<I>, FieldIterator<I>> field_view(FieldIterator<I> begin, FieldIterator<I> end) {
  return std::make_pair(begin, end);
}

template<typename Buffer>
inline
auto field_view(std::vector<Frame<Buffer>> const& dpb) -> std::pair<decltype(field_iterator(dpb.begin(), dpb.end())), decltype(field_iterator(dpb.end(), dpb.end()))> {
  return {field_iterator(dpb.begin(), dpb.end(), dpb.empty() || has_top(dpb.front().pt) ? PicType::Top : PicType::Bottom), field_iterator(dpb.end(), dpb.end())};
}

template<typename Buffer>
inline
auto field_view(std::vector<Frame<Buffer>>& dpb) -> std::pair<decltype(field_iterator(dpb.begin(), dpb.end())), decltype(field_iterator(dpb.end(), dpb.end()))> {
  return {field_iterator(dpb.begin(), dpb.end(), dpb.empty() || has_top(dpb.front().pt) ? PicType::Top : PicType::Bottom), field_iterator(dpb.end(), dpb.end())};
}

struct PicNumDecoder {
  PicNumDecoder(unsigned max_frame_num, PicType pt, unsigned frame_num) : max_frame_num(max_frame_num), pt(pt), frame_num(frame_num) {}
  PicNumDecoder(PictureParameterSet const& pps, SliceHeader const& slice) : PicNumDecoder(MaxFrameNum(pps), slice.pic_type, slice.frame_num) {}

  unsigned max_frame_num;
  PicType pt;
  unsigned frame_num;

  template<typename T>
  int FrameNumWrap(T const& t) const { return FrameNum(t) > frame_num ? FrameNum(t) - max_frame_num : FrameNum(t); }

  template<typename T>
  int PicNum(T const& t) const { return pt == PicType::Frame ? FrameNumWrap(t) : 2 * FrameNumWrap(t) + (pt == t.pt);}

  int CurrPicNum() const { return pt == PicType::Frame ? frame_num : frame_num * 2 + 1; }

  template<typename T>
  unsigned LongTermPicNum(T const& t) const { return pt == PicType::Frame ? LongTermFrameIdx(t) : 2 * LongTermFrameIdx(t) + (pt == t.pt); }

  unsigned MaxPicNum() const { return pt == PicType::Frame ? max_frame_num : max_frame_num * 2; };
};

template<typename Buffer>
std::array<std::vector<Picture<Buffer>>,2> construct_reflists(PictureParameterSet const& pps, SliceHeader const& slice,
  Picture<Buffer> const& curr_pic, std::vector<Frame<Buffer>> const& dpb)
{
  PicNumDecoder c{pps, slice};

  std::vector<Picture<Buffer>> short_term_l0;
  std::vector<Picture<Buffer>> short_term_l1;
  std::vector<Picture<Buffer>> long_term_l;

  if(slice.pic_type == PicType::Frame) {
    std::copy_if(dpb.begin(), dpb.end(), std::back_inserter(short_term_l0), [&](Frame<Buffer> const& f) { return is_short_term_reference(f); });
    std::copy_if(dpb.begin(), dpb.end(), std::back_inserter(long_term_l), [&](Frame<Buffer> const& f) { return is_long_term_reference(f); });
  }
  else {
    auto fields = field_view(dpb);
    std::copy_if(fields.first, fields.second, std::back_inserter(short_term_l0), [&](typename Frame<Buffer>::Field const& f) { return is_short_term_reference(f); });
    std::copy_if(fields.first, fields.second, std::back_inserter(long_term_l), [&](typename Frame<Buffer>::Field const& f) { return is_long_term_reference(f); });
  }

  std::sort(long_term_l.begin(), long_term_l.end(), [&](Picture<Buffer> const& a, Picture<Buffer> const& b) { return LongTermFrameIdx(a) < LongTermFrameIdx(b); });

  if(slice.slice_type == SliceType::P) {
    std::sort(short_term_l0.begin(), short_term_l0.end(), [&](Picture<Buffer> const& a, Picture<Buffer> const& b) { return c.FrameNumWrap(a) > c.FrameNumWrap(b); });
  }
  else if(slice.slice_type == SliceType::B) {
    short_term_l1 = short_term_l0;
    
    std::sort(short_term_l0.begin(), short_term_l0.end(), [&](Picture<Buffer> const& a, Picture<Buffer> const& b) {
      return ((PicOrderCnt(a) <= PicOrderCnt(curr_pic)) ? std::make_tuple(false, -PicOrderCnt(a)-1) : std::make_tuple(true, PicOrderCnt(a)+1)) < 
             ((PicOrderCnt(b) <= PicOrderCnt(curr_pic)) ? std::make_tuple(false, -PicOrderCnt(b)-1) : std::make_tuple(true, PicOrderCnt(b)+1));
    });

    std::sort(short_term_l1.begin(), short_term_l1.end(), [&](Picture<Buffer> const& a, Picture<Buffer> const& b) {
      return ((PicOrderCnt(a) > PicOrderCnt(curr_pic)) ? std::make_tuple(false, PicOrderCnt(a)+1) : std::make_tuple(true, -PicOrderCnt(a)-1)) < 
             ((PicOrderCnt(b) > PicOrderCnt(curr_pic)) ? std::make_tuple(false, PicOrderCnt(b)+1) : std::make_tuple(true, -PicOrderCnt(b)-1));
    });
  }

  std::array<std::vector<Picture<Buffer>>, 2> reflist;

  if(slice.pic_type != PicType::Frame) {
    auto init_ref_pic_list_in_fields= [&](std::vector<Picture<Buffer>>&& short_term, std::vector<Picture<Buffer>>&& long_term, std::vector<Picture<Buffer>>& out) {
      auto fill_reflist = [&](std::vector<Picture<Buffer>>& from, std::vector<Picture<Buffer>>& to) {
        auto pt = curr_pic.pt;
        while(!from.empty()) {
          auto i = std::find_if(from.begin(), from.end(), [&](Picture<Buffer> const& v) { return v.pt == pt; });
          if(i == from.end()) break;

          to.push_back(*i);
          from.erase(i);
          pt = pt == PicType::Top ? PicType::Bottom : PicType::Top;
        }

        to.insert(to.end(), from.begin(), from.end());
      };

      fill_reflist(short_term, out);
      fill_reflist(long_term, out);
    };

    if(slice.slice_type == SliceType::B)
      init_ref_pic_list_in_fields(std::move(short_term_l1), std::vector<Picture<Buffer>>(long_term_l), reflist[1]);
    init_ref_pic_list_in_fields(std::move(short_term_l0), std::move(long_term_l), reflist[0]);
  }
  else {
    reflist[0] = std::move(short_term_l0);
    reflist[0].insert(reflist[0].end(), long_term_l.begin(), long_term_l.end());
    
    if(slice.slice_type == SliceType::B) {
      reflist[1] = std::move(short_term_l1);
      reflist[1].insert(reflist[1].end(), long_term_l.begin(), long_term_l.end());
    }
  }

  if(reflist[1].size() > 1 && reflist[0] == reflist[1]) std::swap(reflist[1][0], reflist[1][1]);
 
  auto ref_pic_list_modification_lx = [&](std::vector<Picture<Buffer>>& reflist, std::vector<std::pair<unsigned, unsigned>> const& ops) {
    int picNum = c.CurrPicNum();
    for(std::size_t i = 0; i != std::min(reflist.size(), ops.size()); ++i) {
      auto pos = reflist.end();

      if(ops[i].first == 0 || ops[i].first == 1) {
        picNum += ((ops[i].first == 0) ? -(ops[i].second + 1) : (ops[i].second + 1));
        if(picNum < 0) picNum += c.MaxPicNum();
        if(picNum > c.MaxPicNum()) picNum -= c.MaxPicNum();
        pos = std::find_if(reflist.begin(), reflist.end(), [&](Picture<Buffer> const& x) {
          return !is_long_term_reference(x) && c.PicNum(x) == ((picNum > c.CurrPicNum()) ? picNum - c.MaxPicNum() : picNum);
        });
      }
      else if(ops[i].first == 2) {
        pos = std::find_if(reflist.begin(), reflist.end(), [&](Picture<Buffer> const& x) {
          return is_long_term_reference(x) && c.LongTermPicNum(x) == ops[i].second;
        });
      }

      if(pos == reflist.end()) throw std::runtime_error("refpic_list_modification - picnum not found in reflist");
    
      if(pos - reflist.begin() >= i) {
        //std::rotate(reflist.begin() + i, pos, pos + 1);
        auto t = std::move(*pos);
        reflist.erase(pos);
        reflist.insert(reflist.begin() + i, std::move(t));
      }
      else {
        reflist.insert(reflist.begin() + i, *pos);
        //reflist.pop_back();
      }
    }
  };

  ref_pic_list_modification_lx(reflist[0], slice.ref_pic_list_modification[0]);
  ref_pic_list_modification_lx(reflist[1], slice.ref_pic_list_modification[1]);
  
  if(reflist[0].size() > slice.num_ref_idx_l0_active_minus1 + 1) reflist[0].erase(reflist[0].begin()+slice.num_ref_idx_l0_active_minus1 + 1, reflist[0].end());
  if(reflist[1].size() > slice.num_ref_idx_l1_active_minus1 + 1) reflist[1].erase(reflist[1].begin()+slice.num_ref_idx_l1_active_minus1 + 1, reflist[1].end());

  return reflist;
}

template<typename I>
inline
void mark_as_long_term_reference(I const& mark, unsigned long_term_frame_idx, std::pair<I,I> const& dpb) {
  auto fields = field_view(dpb.first, dpb.second);
  for(;fields.first != fields.second; ++fields.first)
    if(is_long_term_reference(*fields.first) && LongTermFrameIdx(*fields.first) == long_term_frame_idx && !is_same_frame(fields.first, mark))
      mark_as_unused_for_reference(*fields.first);

  mark_as_long_term_reference(*mark, long_term_frame_idx);
}

template<typename I>
inline
void adaptive_ref_pic_marking(PicNumDecoder const& c, std::vector<MemoryManagementControlOperation> mmcos, I current, std::pair<I,I> const& dpb) {
  std::for_each(mmcos.begin(), mmcos.end(), [&](MemoryManagementControlOperation const& op) {
      if(op.id == 1) {
      auto picnum = c.CurrPicNum() - (op.difference_of_pic_nums_minus1 + 1);
      auto i = std::find_if(dpb.first, dpb.second, [&](typename I::value_type const& a) { return is_short_term_reference(a) && c.PicNum(a) == picnum; });
      if(i != dpb.second)
        mark_as_unused_for_reference(*i);
      else
        throw std::runtime_error("dec_ref_pic_marking: picnum not found");
    }
    else if(op.id == 2) {
      auto i = std::find_if(dpb.first, dpb.second, [&](typename I::value_type const& a) { return is_long_term_reference(a) && c.LongTermPicNum(a) == op.long_term_pic_num; });
      if(i != dpb.second)
        mark_as_unused_for_reference(*i);
      else
        throw std::runtime_error("dec_ref_pic_marking: picnum not found");
    }
    else if(op.id == 3) {
      auto picnum = c.CurrPicNum() - (op.difference_of_pic_nums_minus1 + 1);
      auto i = std::find_if(dpb.first, dpb.second, [&](typename I::value_type const& a) { return is_short_term_reference(a) && c.PicNum(a) == picnum; });
      if(i != dpb.second)
        mark_as_long_term_reference(i, op.long_term_frame_idx, dpb);
      else
        throw std::runtime_error("dec_ref_pic_marking: picnum not found");
    }
    else if(op.id == 4) {
      for(auto i = dpb.first; i != dpb.second; ++i) 
        if(is_long_term_reference(*i) && LongTermFrameIdx(*i) >= op.max_long_term_frame_idx_plus1)
          mark_as_unused_for_reference(*i);
    }
    else if(op.id == 5) {
      for(auto i = dpb.first; i != dpb.second; ++i) {
        if(i != current) mark_as_unused_for_reference(*i);
      }
    }
    else if(op.id == 6) {
      mark_as_long_term_reference(current,op.long_term_frame_idx, dpb);
    } 
  });
}

template<typename Buffer>
inline
void apply_sliding_window_dec_ref_pic_marking(PictureParameterSet const& pps, SliceHeader const& slice, std::vector<Frame<Buffer>>& dpb) {
  auto numShortTerm = std::count_if(dpb.begin(), dpb.end(), [&](Frame<Buffer> const& p) { return p.top.ref == RefType::Short || p.bottom.ref == RefType::Short; });
  auto numLongTerm = std::count_if(dpb.begin(), dpb.end(), [&](Frame<Buffer> const& p) { return p.top.ref == RefType::Long || p.bottom.ref == RefType::Long; });

  PicNumDecoder num{pps, slice};
  
  int n = numShortTerm + numLongTerm - std::max(pps.max_num_ref_frames, 1u);
  n = std::min(n, static_cast<int>(numShortTerm));

  while((n--) > 0) {
    mark_as_unused_for_reference(*std::min_element(dpb.begin(), dpb.end(), [&](Frame<Buffer> const& a, Frame<Buffer> const& b) { 
      return std::make_tuple(!is_short_term_reference(a), num.FrameNumWrap(a)) < std::make_tuple(!is_short_term_reference(b), num.FrameNumWrap(b)); 
    }));
  }
}

template<typename Buffer>
inline
void dec_ref_pic_marking(PictureParameterSet const& pps, SliceHeader const& slice, std::vector<Frame<Buffer>>& dpb) {
  assert(!dpb.empty());

  PicNumDecoder picnum{pps, slice};

  if(slice.IdrPicFlag) 
    for(auto& a: dpb) mark_as_unused_for_reference(a);

  if(slice.pic_type == PicType::Frame)
    mark_as_short_term_reference(dpb.back());
  else
    mark_as_short_term_reference(has_bottom(dpb.back().pt) ? dpb.back().bottom : dpb.back().top);

  if(slice.IdrPicFlag) {
    if(slice.long_term_reference_flag) {
      if(slice.pic_type == PicType::Frame)
        mark_as_long_term_reference(dpb.end()-1, 0, std::make_pair(dpb.begin(), dpb.end()));
      else
        mark_as_long_term_reference(field_iterator(dpb.end()-1, dpb.end(), slice.pic_type), 0, field_view(dpb));
    }
  }
  else if(!slice.memory_management_control_operations.empty()) {
    if(slice.pic_type == PicType::Frame)
      adaptive_ref_pic_marking(picnum, slice.memory_management_control_operations, dpb.end()-1, std::make_pair(dpb.begin(), dpb.end()));
    else
      adaptive_ref_pic_marking(picnum, slice.memory_management_control_operations, field_iterator(dpb.end()-1, dpb.end(), slice.pic_type), field_view(dpb));
  }
  else {
    apply_sliding_window_dec_ref_pic_marking(pps, slice, dpb);
  }
}

inline
std::size_t max_dec_frame_buffering(SequenceParameterSet const& s) {
  static const std::initializer_list<std::pair<unsigned, unsigned>> max_dpb_mbs_map = {
    {10, 396},
    {11, 900},
    {12, 2376},
    {13, 2376},
    {20, 2376},
    {21, 4752},
    {22, 8100},
    {30, 8100},
    {31, 18000},
    {32, 20480},
    {40, 32768},
    {41, 32768},
    {42, 34816},
    {50, 110400},
    {51, 184320},
    {52, 184320}
  };

  auto i = std::find_if(max_dpb_mbs_map.begin(), max_dpb_mbs_map.end(), [&](std::pair<unsigned, unsigned> const& a) { return a.first == s.level_idc; });
  if(i == max_dpb_mbs_map.end()) throw std::runtime_error("unsupported level");

  auto pic_in_mbs = (s.pic_width_in_mbs_minus1 + 1)*(s.pic_height_in_map_units_minus1 + 1)*(s.frame_mbs_only_flag ? 1 : 2);

  return std::min(i->second/pic_in_mbs, 16u);
}

template<typename Buffer>
struct context {
  PictureParameterSet const& pps;
  SliceHeader const& slice;
  std::vector<Frame<Buffer>> dpb;
  Picture<Buffer> curpic;
}; 

}

#endif

