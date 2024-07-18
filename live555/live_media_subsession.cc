#include "rtsp/server/live_media_subsession.h"
#include "rtsp/server/capture_framed_source.h"
#include "rtsp/server/video_encoder_host.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/run_loop.h"

namespace rtsp {
LiveMediaSubSession *LiveMediaSubSession::createNew(UsageEnvironment &env, Boolean reuseFirstSource,
                                                    AVCodecID av_codec_id,
                                                    VideoEncoderHost *video_encoder_host) {
  return new LiveMediaSubSession(env, reuseFirstSource, av_codec_id, video_encoder_host);
}

LiveMediaSubSession::LiveMediaSubSession(UsageEnvironment &env, Boolean reuseFirstSource,
                                         AVCodecID av_codec_id,
                                         VideoEncoderHost *video_encoder_host)
    : OnDemandServerMediaSubsession(env, reuseFirstSource),
      av_codec_id_(av_codec_id),
      video_encoder_host_(video_encoder_host), fDone(false),
      fDummyRTPSink(nullptr), fAuxSDPLine(nullptr),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      weak_factory_(this) {
  LOG(INFO) << __func__;
}

LiveMediaSubSession::~LiveMediaSubSession() {
  LOG(INFO) << __func__;
  setDoneFlag();
  if (fAuxSDPLine) {
    delete[] fAuxSDPLine;
    fAuxSDPLine = nullptr;
  }
  weak_factory_.InvalidateWeakPtrs();
}

void LiveMediaSubSession::setDoneFlag() {
  if (!fDone) {
    LOG(INFO) << __func__;
    fDone = true;
    //force nest runloop quit
    if (!quit_closure_.is_null()) {
      task_runner_->PostTask(FROM_HERE, quit_closure_);
    }
    check_timer_.Stop();
  } else {
    LOG(INFO) << __func__ << ",already done,do nothing";
  }
}

static void afterPlayingDummy(void *clientData) {
  LOG(INFO) << __func__;
  auto *that = reinterpret_cast<LiveMediaSubSession *>(clientData);
  // Signal the event loop that we're done:
  that->afterPlayingDummy1();
}

void LiveMediaSubSession::afterPlayingDummy1() {
  LOG(INFO) << __func__;
  // Signal the event loop that we're done:
  setDoneFlag();
}

void LiveMediaSubSession::checkForAuxSDPLine() {
  LOG(INFO) << __func__;
  char const *dasl;
  if (fAuxSDPLine) {
    // Signal the event loop that we're done:
    LOG(INFO) << __func__ << ",got sdp line,quit loop";
    setDoneFlag();
  } else if (fDummyRTPSink &&
      (dasl = fDummyRTPSink->auxSDPLine()) != nullptr) {
    LOG(INFO) << __func__ << ",auxSDPLine return sdp line,quit loop";
    fAuxSDPLine = strDup(dasl);
    fDummyRTPSink = nullptr;
    // Signal the event loop that we're done:
    setDoneFlag();
  } else if (!fDone) {
    LOG(INFO) << __func__ << ",not have sdp line,and RunLoop not timeout,sleep and check again";
    check_timer_.Start(FROM_HERE,
                       base::TimeDelta::FromMilliseconds(100),
                       base::BindOnce(&LiveMediaSubSession::checkForAuxSDPLine,
                                      weak_factory_.GetWeakPtr()));
  }
}

char const *
LiveMediaSubSession::getAuxSDPLine(RTPSink *rtpSink,
                                   FramedSource *inputSource) {
  LOG(INFO) << __func__;
  // Note: For MPEG-4 video buffer, the 'config' information isn't known
  // until we start reading the Buffer.  This means that "rtpSink"s
  // "auxSDPLine()" will be NULL initially, and we need to start reading
  // data from our buffer until this changes.
  if (fAuxSDPLine) return fAuxSDPLine;
  if (!fDummyRTPSink) {
    fDummyRTPSink = rtpSink;
    fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);
    WaitCompleted();
  }
  return fAuxSDPLine;
}

void LiveMediaSubSession::WaitCompleted() {
  auto t1 = base::TimeTicks::Now();
  LOG(INFO) << __func__ << ",RunLoop start";
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  quit_closure_ = run_loop.QuitClosure();
  check_timer_.Start(FROM_HERE,
                     base::TimeDelta::FromMilliseconds(10),
                     base::BindOnce(&LiveMediaSubSession::checkForAuxSDPLine,
                                    weak_factory_.GetWeakPtr()));
  run_loop.RunWithTimeout(base::TimeDelta::FromSeconds(3));
  check_timer_.Stop();
  fDone = true;
  quit_closure_.Reset();

  LOG(INFO) << __func__ << ",RunLoop end:[" << (base::TimeTicks::Now() - t1).InMicroseconds() << "]";
}

FramedSource *LiveMediaSubSession::createNewStreamSource(unsigned clientSessionId,
                                                         unsigned &estBitrate) {
  LOG(INFO) << __func__ << ",createNewStreamSource clientSessionId:" << clientSessionId;
  estBitrate = 8000; // 1080p 8000kbit/s

  auto frame_source = CaptureFramedSource::createNew(envir(), av_codec_id_, video_encoder_host_);
  if (av_codec_id_ == AV_CODEC_ID_HEVC) {
    return H265VideoStreamDiscreteFramer::createNew(envir(), frame_source);
  }
  return H264VideoStreamDiscreteFramer::createNew(envir(), frame_source);
}

RTPSink *LiveMediaSubSession::createNewRTPSink(Groupsock *rtpGroupsock,
                                               unsigned char rtpPayloadTypeIfDynamic,
                                               FramedSource *inputSource) {
  LOG(INFO) << __func__;
  if (!inputSource) {
    LOG(INFO) << __func__ << ",inputSource is not ready, can not create new rtp sink";
    return nullptr;
  }
  OutPacketBuffer::maxSize = 1920 * 1080 * 2;
  if (av_codec_id_ == AV_CODEC_ID_HEVC) {
    return H265VideoRTPSink::createNew(envir(), rtpGroupsock,
                                       rtpPayloadTypeIfDynamic);
  }
  return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
}
}
