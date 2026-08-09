#include "interpreter.hpp"

namespace aoi {

SpeechInterpreter::SpeechInterpreter(SpeechInterpreterOptions opts)
    : opts_(std::move(opts)),
      segmenter_(SpeechSegmenterOptions{
          [this](const SpeechSegment& seg) {
            if (opts_.onSegment) opts_.onSegment(seg);
          },
          opts_.onState,
          0,                  // fixedSegmentSeconds
          opts_.windowSeconds, // sliding-window mode (W)
          opts_.overlapMs,     // overlap O
      }) {}

SpeechInterpreter::~SpeechInterpreter() { abort(); }

bool SpeechInterpreter::running() const { return segmenter_.running(); }

bool SpeechInterpreter::start() { return segmenter_.start(); }

void SpeechInterpreter::stop() { segmenter_.stop(); }

void SpeechInterpreter::abort() { segmenter_.abort(); }

} // namespace aoi
