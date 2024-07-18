#ifndef RTSP_SERVER_LIVE_MEDIA_SUBSESSION_H_
#define RTSP_SERVER_LIVE_MEDIA_SUBSESSION_H_

#include <memory>
#include <string>
#include "base/macros.h"
#include "build/build_config.h"
#include "base/files/file_path.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "base/single_thread_task_runner.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include <liveMedia.hh>

namespace rtsp {
class VideoEncoderHost;
class LiveMediaSubSession : public OnDemandServerMediaSubsession {
public:
 static LiveMediaSubSession *createNew(UsageEnvironment &env,
                                       Boolean reuseFirstSource,
                                       AVCodecID av_codec_id,
                                       VideoEncoderHost *video_encoder_host);
 void afterPlayingDummy1();
protected:
 explicit LiveMediaSubSession(UsageEnvironment &env,
                              Boolean reuseFirstSource,
                              AVCodecID av_codec_id,
                              VideoEncoderHost *video_encoder_host);
 virtual ~LiveMediaSubSession();
private:
 char const *getAuxSDPLine(RTPSink *rtpSink,
                           FramedSource *inputSource) override;

 FramedSource *createNewStreamSource(unsigned clientSessionId,
                                     unsigned &estBitrate) override;
 // "estBitrate" is the stream's estimated bitrate, in kbps
 RTPSink *createNewRTPSink(Groupsock *rtpGroupsock,
                           unsigned char rtpPayloadTypeIfDynamic,
                           FramedSource *inputSource) override;
 void checkForAuxSDPLine();
 void setDoneFlag();
 void WaitCompleted();

 AVCodecID av_codec_id_;
 VideoEncoderHost *video_encoder_host_;
 bool fDone;        // used when setting up 'SDPlines'
 RTPSink *fDummyRTPSink; // ditto
 char *fAuxSDPLine;
 base::Closure quit_closure_;
 base::OneShotTimer check_timer_;
 scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
 base::WeakPtrFactory<LiveMediaSubSession> weak_factory_;
 DISALLOW_COPY_AND_ASSIGN(LiveMediaSubSession);
};
}
#endif //RTSP_SERVER_LIVE_MEDIA_SUBSESSION_H_