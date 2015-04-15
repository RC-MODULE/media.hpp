#ifndef __mad_hpp_f2033875_556d_4d37_8412_247a6104c5ed__
#define __mad_hpp_f2033875_556d_4d37_8412_247a6104c5ed__

#include <mad.h>

namespace media { namespace mpeg { namespace audio {

std::error_category& error_category() {
 static struct : public std::error_category {
		const char* name() const noexcept { return "mad_error_category"; }
 
    virtual std::string message(int ev) const {
			switch(ev) {
				case MAD_ERROR_BUFLEN: return "MAD_ERROR_BUFLEN";
				case MAD_ERROR_BUFPTR: return "MAD_ERROR_BUFPTR";

				case MAD_ERROR_NOMEM: return "MAD_ERROR_NOMEM";

				case MAD_ERROR_LOSTSYNC: return "MAD_ERROR_LOSTSYNC";
				case MAD_ERROR_BADLAYER: return "MAD_ERROR_BADLAYER";
				case MAD_ERROR_BADBITRATE: return "MAD_ERROR_BADBITRATE";
				case MAD_ERROR_BADSAMPLERATE: return "MAD_ERROR_BADSAMPLERATE";
				case MAD_ERROR_BADEMPHASIS: return "MAD_ERROR_BADEMPHASIS";

				case MAD_ERROR_BADCRC: return "MAD_ERROR_BADCRC";
				case MAD_ERROR_BADBITALLOC: return "MAD_ERROR_BADBITALLOC";
				case MAD_ERROR_BADSCALEFACTOR: return "MAD_ERROR_BADSCALEFACTOR";
				case MAD_ERROR_BADMODE: return "MAD_ERROR_BADMODE";
				case MAD_ERROR_BADFRAMELEN: return "MAD_ERROR_BADFRAMELEN";
				case MAD_ERROR_BADBIGVALUES: return "MAD_ERROR_BADBIGVALUES";
				case MAD_ERROR_BADBLOCKTYPE: return "MAD_ERROR_BADBLOCKTYPE";
				case MAD_ERROR_BADSCFSI: return "MAD_ERROR_BADSCFSI";
				case MAD_ERROR_BADDATAPTR: return "MAD_ERROR_BADDATAPTR";
				case MAD_ERROR_BADPART3LEN: return "MAD_ERROR_BADPART3LEN";
				case MAD_ERROR_BADHUFFTABLE: return "MAD_ERROR_BADHUFFTABLE";
				case MAD_ERROR_BADHUFFDATA: return "MAD_ERROR_BADHUFFDATA";
				case MAD_ERROR_BADSTEREO: return "MAD_ERROR_BADSTEREO";
        default: return "MAD_UNKNOWN_ERROR";
      }		
		}
	} cat;
  return cat;
}

struct decoder {
  decoder() {
    mad_stream_init(&stream);
    mad_frame_init(&frame);
    mad_synth_init(&synth);
  }

  ~decoder() {
    mad_synth_finish(&synth);
    mad_frame_finish(&frame);
    mad_stream_finish(&stream);
  }

  decoder(decoder const&) = delete;
  decoder& operator = (decoder const&) = delete;

  decoder(decoder&&) = default;
  decoder& operator = (decoder&&) = default;

  using sample = std::pair<std::int16_t, std::int16_t>;

  template<typename AsioConstBufferSequence>
  std::vector<sample> operator()(AsioConstBufferSequence const& data) {
    push_back_buffer_sequence(buffer, data);
    //buffer.reserve(buffer.size() + MAD_BUFFER_GUARD); 

    mad_stream_buffer(&stream, &buffer[0], buffer.size());

    std::vector<sample> r;
    for(;;) {
      while(mad_frame_decode(&frame, &stream)) {
        if(!MAD_RECOVERABLE(stream.error)) {
          if(stream.error == MAD_ERROR_BUFLEN) {
            buffer.erase(buffer.begin(), buffer.begin() + (stream.next_frame - stream.buffer));
            return r;
          }

          throw std::system_error(stream.error, error_category());
        }
      }
 
      mad_synth_frame(&synth, &frame);
    
      auto p = r.size();
      r.resize(r.size() + synth.pcm.length);
      for(size_t i = 0; i != synth.pcm.length; ++i)
        r[p++] = {mad_to_int16(synth.pcm.samples[0][i]), mad_to_int16(synth.pcm.samples[1][i])};
    }    
  }

  std::size_t sample_rate() const { return synth.pcm.samplerate; }
  std::uint16_t length() const { return synth.pcm.length; }  // this is either 384 or 1152
  int channels() const { return synth.pcm.channels; } // this is either 1 or 2
private:
  static std::int16_t mad_to_int16(mad_fixed_t f) {
    if(f >= MAD_F_ONE)
      return std::numeric_limits<std::int16_t>::max();
    if(f <= -MAD_F_ONE)
      return std::numeric_limits<std::int16_t>::min();

    return f >> (MAD_F_FRACBITS - 15);
  }

  static sample mad_to_sample(mad_fixed_t const* d) { return {mad_to_int16(d[0]), mad_to_int16(d[1])}; }
  
  mad_stream stream;
  mad_frame frame;
  mad_synth synth;
  std::vector<std::uint8_t> buffer;
};


}}}


#endif
