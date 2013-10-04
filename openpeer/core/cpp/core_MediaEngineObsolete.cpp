/*
 
 Copyright (c) 2013, SMB Phone Inc.
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 The views and conclusions contained in the software and documentation are those
 of the authors and should not be interpreted as representing official policies,
 either expressed or implied, of the FreeBSD Project.
 
 */

#include <openpeer/core/internal/core_MediaEngineObsolete.h>
#include <openpeer/core/internal/core_Stack.h>
#include <openpeer/core/internal/core_IConversationThreadParser.h>
#include <openpeer/core/ILogger.h>

#include <zsLib/helpers.h>

#include <boost/thread.hpp>

#include <video_capture_factory.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#ifdef TARGET_OS_IPHONE
#include <sys/sysctl.h>
#endif

#define OPENPEER_MEDIA_ENGINE_VOICE_CODEC_ISAC
//#define OPENPEER_MEDIA_ENGINE_VOICE_CODEC_OPUS
#define OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL (-1)
#define OPENPEER_MEDIA_ENGINE_MTU (576)

namespace openpeer { namespace core { ZS_DECLARE_SUBSYSTEM(openpeer_webrtc_obsolete) } }

namespace openpeer
{
  namespace core
  {
    using zsLib::Stringize;
    
    namespace internal
    {
      typedef zsLib::ThreadPtr ThreadPtr;
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark IMediaEngineForStack
#pragma mark
      
      //-----------------------------------------------------------------------
      void IMediaEngineForStackObsolete::setup(IMediaEngineDelegateObsoletePtr delegate)
      {
        MediaEngineObsolete::setup(delegate);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark IMediaEngineForCallTransport
#pragma mark
      
      //-----------------------------------------------------------------------
      MediaEngineObsoletePtr IMediaEngineForCallTransportObsolete::singleton()
      {
        return MediaEngineObsolete::singleton();
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete
#pragma mark
      
      //-----------------------------------------------------------------------
      MediaEngineObsolete::MediaEngineObsolete(
                               IMessageQueuePtr queue,
                               IMediaEngineDelegateObsoletePtr delegate
                               ) :
      MessageQueueAssociator(queue),
      mError(0),
      mMtu(OPENPEER_MEDIA_ENGINE_MTU),
      mID(zsLib::createPUID()),
      mDelegate(IMediaEngineDelegateObsoleteProxy::createWeak(delegate)),
      mEcEnabled(false),
      mAgcEnabled(false),
      mNsEnabled(false),
      mVoiceRecordFile(""),
      mDefaultVideoOrientation(IMediaEngineObsolete::VideoOrientation_LandscapeLeft),
      mRecordVideoOrientation(IMediaEngineObsolete::VideoOrientation_LandscapeLeft),
      mVoiceChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
      mVoiceTransport(&mRedirectVoiceTransport),
      mVideoChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
      mVideoTransport(&mRedirectVideoTransport),
      mCaptureId(0),
      mCameraType(CameraType_Front),
      mVoiceEngine(NULL),
      mVoiceBase(NULL),
      mVoiceCodec(NULL),
      mVoiceNetwork(NULL),
      mVoiceRtpRtcp(NULL),
      mVoiceAudioProcessing(NULL),
      mVoiceVolumeControl(NULL),
      mVoiceHardware(NULL),
      mVoiceFile(NULL),
      mVoiceEngineReady(false),
      mVcpm(NULL),
      mVideoEngine(NULL),
      mVideoBase(NULL),
      mVideoNetwork(NULL),
      mVideoRender(NULL),
      mVideoCapture(NULL),
      mVideoRtpRtcp(NULL),
      mVideoCodec(NULL),
      mVideoFile(NULL),
      mVideoEngineReady(false),
      mFaceDetection(false),
      mCaptureRenderView(NULL),
      mChannelRenderView(NULL),
      mRedirectVoiceTransport("voice"),
      mRedirectVideoTransport("video"),
      mLifetimeWantAudio(false),
      mLifetimeWantVideoCapture(false),
      mLifetimeWantVideoChannel(false),
      mLifetimeWantRecordVideoCapture(false),
      mLifetimeHasAudio(false),
      mLifetimeHasVideoCapture(false),
      mLifetimeHasVideoChannel(false),
      mLifetimeHasRecordVideoCapture(false),
      mLifetimeInProgress(false),
      mLifetimeWantCameraType(CameraType_Front),
      mLifetimeContinuousVideoCapture(false),
      mLifetimeVideoRecordFile(""),
      mLifetimeSaveVideoToLibrary(false)
      {
#ifdef TARGET_OS_IPHONE
        int name[] = {CTL_HW, HW_MACHINE};
        size_t size;
        sysctl(name, 2, NULL, &size, NULL, 0);
        char *machine = (char *)malloc(size);
        sysctl(name, 2, machine, &size, NULL, 0);
        mMachineName = machine;
        free(machine);
#endif
      }
      
      MediaEngineObsolete::MediaEngineObsolete(Noop) :
      Noop(true),
      MessageQueueAssociator(IMessageQueuePtr()),
      mError(0),
      mMtu(OPENPEER_MEDIA_ENGINE_MTU),
      mID(zsLib::createPUID()),
      mEcEnabled(false),
      mAgcEnabled(false),
      mNsEnabled(false),
      mVoiceRecordFile(""),
      mDefaultVideoOrientation(IMediaEngineObsolete::VideoOrientation_LandscapeLeft),
      mRecordVideoOrientation(IMediaEngineObsolete::VideoOrientation_LandscapeLeft),
      mVoiceChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
      mVoiceTransport(NULL),
      mVideoChannel(OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL),
      mVideoTransport(NULL),
      mCaptureId(0),
      mCameraType(CameraType_Front),
      mVoiceEngine(NULL),
      mVoiceBase(NULL),
      mVoiceCodec(NULL),
      mVoiceNetwork(NULL),
      mVoiceRtpRtcp(NULL),
      mVoiceAudioProcessing(NULL),
      mVoiceVolumeControl(NULL),
      mVoiceHardware(NULL),
      mVoiceFile(NULL),
      mVoiceEngineReady(false),
      mVcpm(NULL),
      mVideoEngine(NULL),
      mVideoBase(NULL),
      mVideoNetwork(NULL),
      mVideoRender(NULL),
      mVideoCapture(NULL),
      mVideoRtpRtcp(NULL),
      mVideoCodec(NULL),
      mVideoFile(NULL),
      mVideoEngineReady(false),
      mFaceDetection(false),
      mCaptureRenderView(NULL),
      mChannelRenderView(NULL),
      mRedirectVoiceTransport("voice"),
      mRedirectVideoTransport("video"),
      mLifetimeWantAudio(false),
      mLifetimeWantVideoCapture(false),
      mLifetimeWantVideoChannel(false),
      mLifetimeWantRecordVideoCapture(false),
      mLifetimeHasAudio(false),
      mLifetimeHasVideoCapture(false),
      mLifetimeHasVideoChannel(false),
      mLifetimeHasRecordVideoCapture(false),
      mLifetimeInProgress(false),
      mLifetimeWantCameraType(CameraType_Front),
      mLifetimeContinuousVideoCapture(false),
      mLifetimeVideoRecordFile(""),
      mLifetimeSaveVideoToLibrary(false)
      {
#ifdef TARGET_OS_IPHONE
        int name[] = {CTL_HW, HW_MACHINE};
        size_t size;
        sysctl(name, 2, NULL, &size, NULL, 0);
        char *machine = (char *)malloc(size);
        sysctl(name, 2, machine, &size, NULL, 0);
        mMachineName = machine;
        free(machine);
#endif
      }
      
      //-----------------------------------------------------------------------
      MediaEngineObsolete::~MediaEngineObsolete()
      {
        if(isNoop()) return;
        
        destroyMediaEngine();
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::init()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("init media engine"))
        
        mVoiceEngine = webrtc::VoiceEngine::Create();
        if (mVoiceEngine == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create voice engine"))
          return;
        }
        mVoiceBase = webrtc::VoEBase::GetInterface(mVoiceEngine);
        if (mVoiceBase == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice base"))
          return;
        }
        mVoiceCodec = webrtc::VoECodec::GetInterface(mVoiceEngine);
        if (mVoiceCodec == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice codec"))
          return;
        }
        mVoiceNetwork = webrtc::VoENetwork::GetInterface(mVoiceEngine);
        if (mVoiceNetwork == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice network"))
          return;
        }
        mVoiceRtpRtcp = webrtc::VoERTP_RTCP::GetInterface(mVoiceEngine);
        if (mVoiceRtpRtcp == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice RTP/RTCP"))
          return;
        }
        mVoiceAudioProcessing = webrtc::VoEAudioProcessing::GetInterface(mVoiceEngine);
        if (mVoiceAudioProcessing == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for audio processing"))
          return;
        }
        mVoiceVolumeControl = webrtc::VoEVolumeControl::GetInterface(mVoiceEngine);
        if (mVoiceVolumeControl == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for volume control"))
          return;
        }
        mVoiceHardware = webrtc::VoEHardware::GetInterface(mVoiceEngine);
        if (mVoiceHardware == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for audio hardware"))
          return;
        }
        mVoiceFile = webrtc::VoEFile::GetInterface(mVoiceEngine);
        if (mVoiceFile == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for voice file"))
          return;
        }
        
        mError = mVoiceBase->Init();
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to initialize voice base (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        } else if (mVoiceBase->LastError() > 0) {
          ZS_LOG_WARNING(Detail, log("an error has occured during voice base init (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
        }
        mError = mVoiceBase->RegisterVoiceEngineObserver(*this);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to register voice engine observer (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
        
        mVideoEngine = webrtc::VideoEngine::Create();
        if (mVideoEngine == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to create video engine"))
          return;
        }
        
        mVideoBase = webrtc::ViEBase::GetInterface(mVideoEngine);
        if (mVideoBase == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video base"))
          return;
        }
        mVideoCapture = webrtc::ViECapture::GetInterface(mVideoEngine);
        if (mVideoCapture == NULL) {
          ZS_LOG_ERROR(Detail, log("failed get interface for video capture"))
          return;
        }
        mVideoRtpRtcp = webrtc::ViERTP_RTCP::GetInterface(mVideoEngine);
        if (mVideoRtpRtcp == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video RTP/RTCP"))
          return;
        }
        mVideoNetwork = webrtc::ViENetwork::GetInterface(mVideoEngine);
        if (mVideoNetwork == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video network"))
          return;
        }
        mVideoRender = webrtc::ViERender::GetInterface(mVideoEngine);
        if (mVideoRender == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video render"))
          return;
        }
        mVideoCodec = webrtc::ViECodec::GetInterface(mVideoEngine);
        if (mVideoCodec == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video codec"))
          return;
        }
        mVideoFile = webrtc::ViEFile::GetInterface(mVideoEngine);
        if (mVideoFile == NULL) {
          ZS_LOG_ERROR(Detail, log("failed to get interface for video file"))
          return;
        }
        
        mError = mVideoBase->Init();
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to initialize video base (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        } else if (mVideoBase->LastError() > 0) {
          ZS_LOG_WARNING(Detail, log("an error has occured during video base init (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
        }
        
        mError = mVideoBase->SetVoiceEngine(mVoiceEngine);
        if (mError < 0) {
          ZS_LOG_ERROR(Detail, log("failed to set voice engine for video base (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        }
        
        setLogLevel();
        
        Log::Level logLevel = ZS_GET_LOG_LEVEL();
        
        unsigned int traceFilter;
        switch (logLevel) {
          case Log::None:
            traceFilter = webrtc::kTraceNone;
            break;
          case Log::Basic:
            traceFilter = webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical;
            break;
          case Log::Detail:
            traceFilter = webrtc::kTraceStateInfo | webrtc::kTraceWarning | webrtc::kTraceError | webrtc::kTraceCritical | webrtc::kTraceApiCall;
            break;
          case Log::Debug:
            traceFilter = webrtc::kTraceDefault | webrtc::kTraceDebug | webrtc::kTraceInfo;
            break;
          case Log::Trace:
            traceFilter = webrtc::kTraceAll;
            break;
          default:
            traceFilter = webrtc::kTraceNone;
            break;
        }
        
        if (logLevel != Log::None) {
          mError = mVoiceEngine->SetTraceFilter(traceFilter);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace filter for voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVoiceEngine->SetTraceCallback(this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace callback for voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoEngine->SetTraceFilter(traceFilter);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace filter for video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoEngine->SetTraceCallback(this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to set trace callback for video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::destroyMediaEngine()
      {
        // scope: delete voice engine
        {
          if (mVoiceBase) {
            mError = mVoiceBase->DeRegisterVoiceEngineObserver();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to deregister voice engine observer (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
            mError = mVoiceBase->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice base (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceCodec) {
            mError = mVoiceCodec->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice codec (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceNetwork) {
            mError = mVoiceNetwork->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice network (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceRtpRtcp) {
            mError = mVoiceRtpRtcp->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice RTP/RTCP (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceAudioProcessing) {
            mError = mVoiceAudioProcessing->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release audio processing (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceVolumeControl) {
            mError = mVoiceVolumeControl->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release volume control (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceHardware) {
            mError = mVoiceHardware->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release audio hardware (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVoiceFile) {
            mError = mVoiceFile->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release voice file (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (!VoiceEngine::Delete(mVoiceEngine)) {
            ZS_LOG_ERROR(Detail, log("failed to delete voice engine"))
            return;
          }
        }
        
        // scope; delete video engine
        {
          if (mVideoBase) {
            mError = mVideoBase->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video base (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVideoNetwork) {
            mError = mVideoNetwork->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video network (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVideoRender) {
            mError = mVideoRender->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video render (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVideoCapture) {
            mError = mVideoCapture->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video capture (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVideoRtpRtcp) {
            mError = mVideoRtpRtcp->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video RTP/RTCP (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVideoCodec) {
            mError = mVideoCodec->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video codec (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (mVideoFile) {
            mError = mVideoFile->Release();
            if (mError < 0) {
              ZS_LOG_ERROR(Detail, log("failed to release video file (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
          }
          
          if (!VideoEngine::Delete(mVideoEngine)) {
            ZS_LOG_ERROR(Detail, log("failed to delete video engine"))
            return;
          }
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setLogLevel()
      {
        //        ILogger::setLogLevel("openpeer_webrtc", ILogger::Detail);
      }
      
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => IMediaEngine
#pragma mark
      
      //-----------------------------------------------------------------------
      MediaEngineObsoletePtr MediaEngineObsolete::create(IMediaEngineDelegateObsoletePtr delegate)
      {
        MediaEngineObsoletePtr pThis(new MediaEngineObsolete(IStackForInternal::queueCore(), delegate));
        pThis->mThisWeak = pThis;
        pThis->init();
        return pThis;
      }
      
      //-----------------------------------------------------------------------
      MediaEngineObsoletePtr MediaEngineObsolete::singleton(IMediaEngineDelegateObsoletePtr delegate)
      {
        static MediaEngineObsoletePtr engine = IMediaEngineFactoryObsolete::singleton().createMediaEngine(delegate);
        return engine;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::setDefaultVideoOrientation(VideoOrientations orientation)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set default video orientation - ") + IMediaEngineObsolete::toString(orientation))
        
        mDefaultVideoOrientation = orientation;
      }
      
      //-------------------------------------------------------------------------
      MediaEngineObsolete::VideoOrientations MediaEngineObsolete::getDefaultVideoOrientation()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get default video orientation"))
        
        return mDefaultVideoOrientation;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::setRecordVideoOrientation(VideoOrientations orientation)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set record video orientation - ") + IMediaEngineObsolete::toString(orientation))
        
        mRecordVideoOrientation = orientation;
      }
      
      //-------------------------------------------------------------------------
      MediaEngineObsolete::VideoOrientations MediaEngineObsolete::getRecordVideoOrientation()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get record video orientation"))
        
        return mRecordVideoOrientation;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setVideoOrientation()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set video orientation and codec parameters"))
        
        if (mVideoChannel == OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL) {
          mError = setVideoCaptureRotation();
        } else {
          mError = setVideoCodecParameters();
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setCaptureRenderView(void *renderView)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set capture render view"))
        
        mCaptureRenderView = renderView;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setChannelRenderView(void *renderView)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set channel render view"))
        
        mChannelRenderView = renderView;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setEcEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set EC enabled - value: ") + (enabled ? "true" : "false"))
        
        webrtc::EcModes ecMode = getEcMode();
        if (ecMode == webrtc::kEcUnchanged) {
          return;
        }
        mError = mVoiceAudioProcessing->SetEcStatus(enabled, ecMode);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller status (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
        if (ecMode == webrtc::kEcAecm && enabled) {
          mError = mVoiceAudioProcessing->SetAecmMode(webrtc::kAecmSpeakerphone);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller mobile mode (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
        }
        
        mEcEnabled = enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setAgcEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set AGC enabled - value: ") + (enabled ? "true" : "false"))
        
        mError = mVoiceAudioProcessing->SetAgcStatus(enabled, webrtc::kAgcAdaptiveDigital);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set automatic gain control status (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
        mAgcEnabled = enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setNsEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set NS enabled - value: ") + (enabled ? "true" : "false"))
        
        mError = mVoiceAudioProcessing->SetNsStatus(enabled, webrtc::kNsLowSuppression);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set noise suppression status (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
        mNsEnabled = enabled;
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::setVoiceRecordFile(String fileName)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set voice record file - value: ") + fileName)
        
        mVoiceRecordFile = fileName;
      }
      
      //-------------------------------------------------------------------------
      String MediaEngineObsolete::getVoiceRecordFile() const
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get voice record file - value: ") + mVoiceRecordFile)
        
        return mVoiceRecordFile;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setMuteEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set microphone mute enabled - value: ") + (enabled ? "true" : "false"))
        
        mError = mVoiceVolumeControl->SetInputMute(-1, enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngineObsolete::getMuteEnabled()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get microphone mute enabled"))
        
        bool enabled;
        mError = mVoiceVolumeControl->GetInputMute(-1, enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set microphone mute (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return false;
        }
        
        return enabled;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setLoudspeakerEnabled(bool enabled)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set loudspeaker enabled - value: ") + (enabled ? "true" : "false"))
        
        mError = mVoiceHardware->SetLoudspeakerStatus(enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set loudspeaker (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngineObsolete::getLoudspeakerEnabled()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get loudspeaker enabled"))
        
        bool enabled;
        mError = mVoiceHardware->GetLoudspeakerStatus(enabled);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get loudspeaker (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return false;
        }
        
        return enabled;
      }
      
      //-----------------------------------------------------------------------
      IMediaEngineObsolete::OutputAudioRoutes MediaEngineObsolete::getOutputAudioRoute()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get output audio route"))
        
        OutputAudioRoute route;
        mError = mVoiceHardware->GetOutputAudioRoute(route);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get output audio route (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return OutputAudioRoute_BuiltInSpeaker;
        }
        
        switch (route) {
          case webrtc::kOutputAudioRouteHeadphone:
            return OutputAudioRoute_Headphone;
          case webrtc::kOutputAudioRouteBuiltInReceiver:
            return OutputAudioRoute_BuiltInReceiver;
          case webrtc::kOutputAudioRouteBuiltInSpeaker:
            return OutputAudioRoute_BuiltInSpeaker;
          default:
            return OutputAudioRoute_BuiltInSpeaker;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setContinuousVideoCapture(bool continuousVideoCapture)
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("set continuous video capture - value: ") + (continuousVideoCapture ? "true" : "false"))
        
        mLifetimeContinuousVideoCapture = continuousVideoCapture;
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngineObsolete::getContinuousVideoCapture()
      {
        AutoRecursiveLock lock(mLifetimeLock);
        
        ZS_LOG_DEBUG(log("get continuous video capture"))
        
        return mLifetimeContinuousVideoCapture;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setFaceDetection(bool faceDetection)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("set face detection - value: ") + (faceDetection ? "true" : "false"))
        
        mFaceDetection = faceDetection;
      }
      
      //-----------------------------------------------------------------------
      bool MediaEngineObsolete::getFaceDetection()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("get face detection"))
        
        return mFaceDetection;
      }
      
      //-----------------------------------------------------------------------
      IMediaEngineObsolete::CameraTypes MediaEngineObsolete::getCameraType() const
      {
        AutoRecursiveLock lock(mLifetimeLock);  // WARNING: THIS IS THE LIFETIME LOCK AND NOT THE MAIN OBJECT LOCK
        return mLifetimeWantCameraType;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setCameraType(CameraTypes type)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantCameraType = type;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::startVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoCapture = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::stopVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoCapture = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::startRecordVideoCapture(String fileName, bool saveToLibrary)
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantRecordVideoCapture = true;
          mLifetimeVideoRecordFile = fileName;
          mLifetimeSaveVideoToLibrary = saveToLibrary;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::stopRecordVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantRecordVideoCapture = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::getVideoTransportStatistics(RtpRtcpStatistics &stat)
      {
        AutoRecursiveLock lock(mLock);
        
        unsigned short fractionLost;
        unsigned int cumulativeLost;
        unsigned int extendedMax;
        unsigned int jitter;
        int rttMs;
        
        mError = mVideoRtpRtcp->GetReceivedRTCPStatistics(mVideoChannel, fractionLost, cumulativeLost, extendedMax, jitter, rttMs);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get received RTCP statistics for video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        
        unsigned int bytesSent;
        unsigned int packetsSent;
        unsigned int bytesReceived;
        unsigned int packetsReceived;
        
        mError = mVideoRtpRtcp->GetRTPStatistics(mVideoChannel, bytesSent, packetsSent, bytesReceived, packetsReceived);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get RTP statistics for video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        
        stat.fractionLost = fractionLost;
        stat.cumulativeLost = cumulativeLost;
        stat.extendedMax = extendedMax;
        stat.jitter = jitter;
        stat.rttMs = rttMs;
        stat.bytesSent = bytesSent;
        stat.packetsSent = packetsSent;
        stat.bytesReceived = bytesReceived;
        stat.packetsReceived = packetsReceived;
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::getVoiceTransportStatistics(RtpRtcpStatistics &stat)
      {
        AutoRecursiveLock lock(mLock);
        
        webrtc::CallStatistics callStat;
        
        mError = mVoiceRtpRtcp->GetRTCPStatistics(mVoiceChannel, callStat);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("failed to get RTCP statistics for voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return mError;
        }
        
        stat.fractionLost = callStat.fractionLost;
        stat.cumulativeLost = callStat.cumulativeLost;
        stat.extendedMax = callStat.extendedMax;
        stat.jitter = callStat.jitterSamples;
        stat.rttMs = callStat.rttMs;
        stat.bytesSent = callStat.bytesSent;
        stat.packetsSent = callStat.packetsSent;
        stat.bytesReceived = callStat.bytesReceived;
        stat.packetsReceived = callStat.packetsReceived;
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => IMediaEngineForStack
#pragma mark
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::setup(IMediaEngineDelegateObsoletePtr delegate)
      {
        singleton(delegate);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => IMediaEngineForCallTransport
#pragma mark
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::startVoice()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAudio = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::stopVoice()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantAudio = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::startVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoChannel = true;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::stopVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLifetimeLock);
          mLifetimeWantVideoChannel = false;
        }
        
        ThreadPtr(new boost::thread(boost::ref(*((mThisWeak.lock()).get()))));
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::registerVoiceExternalTransport(Transport &transport)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("register voice external transport"))
        
        mRedirectVoiceTransport.redirect(&transport);
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::deregisterVoiceExternalTransport()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("deregister voice external transport"))
        
        mRedirectVoiceTransport.redirect(NULL);
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::receivedVoiceRTPPacket(const void *data, unsigned int length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVoiceEngineReady)
            channel = mVoiceChannel;
        }
        
        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("voice channel is not ready yet"))
          return -1;
        }
        
        mError = mVoiceNetwork->ReceivedRTPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received voice RTP packet failed (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return mError;
        }
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::receivedVoiceRTCPPacket(const void* data, unsigned int length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVoiceEngineReady)
            channel = mVoiceChannel;
        }
        
        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("voice channel is not ready yet"))
          return -1;
        }
        
        mError = mVoiceNetwork->ReceivedRTCPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received voice RTCP packet failed (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return mError;
        }
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::registerVideoExternalTransport(Transport &transport)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("register video external transport"))
        
        mRedirectVideoTransport.redirect(&transport);
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::deregisterVideoExternalTransport()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("deregister video external transport"))
        
        mRedirectVideoTransport.redirect(NULL);
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::receivedVideoRTPPacket(const void *data, const int length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVideoEngineReady)
            channel = mVideoChannel;
        }
        
        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("video channel is not ready yet"))
          return -1;
        }
        
        mError = mVideoNetwork->ReceivedRTPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received video RTP packet failed (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::receivedVideoRTCPPacket(const void *data, const int length)
      {
        int channel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          if (mVideoEngineReady)
            channel = mVideoChannel;
        }
        
        if (OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL == channel) {
          ZS_LOG_WARNING(Debug, log("video channel is not ready yet"))
          return -1;
        }
        
        mError = mVideoNetwork->ReceivedRTCPPacket(channel, data, length);
        if (0 != mError) {
          ZS_LOG_ERROR(Detail, log("received video RTCP packet failed (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        
        return 0;
      }
      
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => TraceCallback
#pragma mark
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::Print(const webrtc::TraceLevel level, const char *traceString, const int length)
      {
        switch (level) {
          case webrtc::kTraceApiCall:
          case webrtc::kTraceStateInfo:
            ZS_LOG_DETAIL(log(traceString))
            break;
          case webrtc::kTraceDebug:
          case webrtc::kTraceInfo:
            ZS_LOG_DEBUG(log(traceString))
            break;
          case webrtc::kTraceWarning:
            ZS_LOG_WARNING(Detail, log(traceString))
            break;
          case webrtc::kTraceError:
            ZS_LOG_ERROR(Detail, log(traceString))
            break;
          case webrtc::kTraceCritical:
            ZS_LOG_FATAL(Detail, log(traceString))
            break;
          case webrtc::kTraceModuleCall:
          case webrtc::kTraceMemory:
          case webrtc::kTraceTimer:
          case webrtc::kTraceStream:
            ZS_LOG_TRACE(log(traceString))
            break;
          default:
            ZS_LOG_TRACE(log(traceString))
            break;
        }
      }
      
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
      //---------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => VoiceEngineObserver
#pragma mark
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::CallbackOnError(const int errCode, const int channel)
      {
        ZS_LOG_ERROR(Detail, log("Voice engine error: ") + Stringize<INT>(errCode).string() + ")")
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::CallbackOnOutputAudioRouteChange(const webrtc::OutputAudioRoute inRoute)
      {
        if (!mDelegate) {
          ZS_LOG_WARNING(Detail, log("audio route change callback igored as delegate was not specified"))
          return;
        }
        
        OutputAudioRoutes route = IMediaEngineObsolete::OutputAudioRoute_Headphone;
        
        switch (inRoute) {
          case webrtc::kOutputAudioRouteHeadphone:        route = IMediaEngineObsolete::OutputAudioRoute_Headphone;  break;
          case webrtc::kOutputAudioRouteBuiltInReceiver:  route = IMediaEngineObsolete::OutputAudioRoute_BuiltInReceiver; break;
          case webrtc::kOutputAudioRouteBuiltInSpeaker:   route = IMediaEngineObsolete::OutputAudioRoute_BuiltInSpeaker; break;
          default: {
            ZS_LOG_WARNING(Basic, log("media route changed to unknown type") + ", value=" + Stringize<typeof(inRoute)>(inRoute).string())
            break;
          }
        }
        
        try {
          if (mDelegate)
            mDelegate->onMediaEngineAudioRouteChanged(route);
        } catch (IMediaEngineDelegateObsoleteProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
        
        ZS_LOG_DEBUG(log("Audio output route changed") + ", route=" + IMediaEngineObsolete::toString(route))
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => ViECaptureObserver
#pragma mark
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::BrightnessAlarm(const int capture_id, const webrtc::Brightness brightness)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::CapturedFrameRate(const int capture_id, const unsigned char frame_rate)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::NoPictureAlarm(const int capture_id, const webrtc::CaptureAlarm alarm)
      {
        
      }
      
      //-------------------------------------------------------------------------
      void MediaEngineObsolete::FaceDetected(const int capture_id)
      {
        try {
          if (mDelegate)
            mDelegate->onMediaEngineFaceDetected();
        } catch (IMediaEngineDelegateObsoleteProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete => (internal)
#pragma mark
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::operator()()
      {
#ifndef _LINUX
# ifdef __QNX__
        pthread_setname_np(pthread_self(), "org.openpeer.core.mediaEngine");
# else
        pthread_setname_np("org.openpeer.core.mediaEngine");
# endif
#endif
        ZS_LOG_DEBUG(log("media engine lifetime thread spawned"))
        
        bool repeat = false;
        
        bool firstAttempt = true;
        
        bool wantAudio = false;
        bool wantVideoCapture = false;
        bool wantVideoChannel = false;
        bool wantRecordVideoCapture = false;
        bool hasAudio = false;
        bool hasVideoCapture = false;
        bool hasVideoChannel = false;
        bool hasRecordVideoCapture = false;
        CameraTypes wantCameraType = IMediaEngineObsolete::CameraType_None;
        String videoRecordFile;
        bool saveVideoToLibrary;
        
        // attempt to get the lifetime lock
        while (true)
        {
          if (!firstAttempt) {
            boost::thread::yield();       // do not hammer CPU
          }
          firstAttempt = false;
          
          AutoRecursiveLock lock(mLifetimeLock);
          if (mLifetimeInProgress) {
            ZS_LOG_WARNING(Debug, log("could not obtain media lifetime lock"))
            continue;
          }
          
          mLifetimeInProgress = true;
          
          if (mLifetimeWantVideoChannel)
            mLifetimeWantVideoCapture = true;
          else if (mLifetimeHasVideoChannel && !mLifetimeContinuousVideoCapture)
            mLifetimeWantVideoCapture = false;
          if (!mLifetimeWantVideoCapture)
            mLifetimeWantRecordVideoCapture = false;
          wantAudio = mLifetimeWantAudio;
          wantVideoCapture = mLifetimeWantVideoCapture;
          wantVideoChannel = mLifetimeWantVideoChannel;
          wantRecordVideoCapture = mLifetimeWantRecordVideoCapture;
          hasAudio = mLifetimeHasAudio;
          hasVideoCapture = mLifetimeHasVideoCapture;
          hasVideoChannel = mLifetimeHasVideoChannel;
          hasRecordVideoCapture = mLifetimeHasRecordVideoCapture;
          wantCameraType = mLifetimeWantCameraType;
          videoRecordFile = mLifetimeVideoRecordFile;
          saveVideoToLibrary = mLifetimeSaveVideoToLibrary;
          break;
        }
        
        {
          AutoRecursiveLock lock(mLock);
          
          if (wantVideoCapture) {
            if (wantCameraType != mCameraType) {
              ZS_LOG_DEBUG(log("camera type needs to change") + ", was=" + IMediaEngineObsolete::toString(mCameraType) + ", desired=" + IMediaEngineObsolete::toString(wantCameraType))
              mCameraType = wantCameraType;
              if (hasVideoCapture) {
                ZS_LOG_DEBUG(log("video capture must be stopped first before camera type can be swapped (will try again)"))
                wantVideoCapture = false;  // pretend that we don't want video so it will be stopped
                repeat = true;      // repeat this thread operation again to start video back up again after
                if (hasVideoChannel) {
                  ZS_LOG_DEBUG(log("video channel must be stopped first before camera type can be swapped (will try again)"))
                  wantVideoChannel = false;  // pretend that we don't want video so it will be stopped
                }
              }
            }
          }
          
          if (wantVideoCapture) {
            if (!hasVideoCapture) {
              internalStartVideoCapture();
            }
          }
          
          if (wantRecordVideoCapture) {
            if (!hasRecordVideoCapture) {
              internalStartRecordVideoCapture(videoRecordFile, saveVideoToLibrary);
            }
          } else {
            if (hasRecordVideoCapture) {
              internalStopRecordVideoCapture();
            }
          }
          
          if (wantAudio) {
            if (!hasAudio) {
              internalStartVoice();
            }
          } else {
            if (hasAudio) {
              internalStopVoice();
            }
          }
          
          if (wantVideoChannel) {
            if (!hasVideoChannel) {
              internalStartVideoChannel();
            }
          } else {
            if (hasVideoChannel) {
              internalStopVideoChannel();
            }
          }
          
          if (!wantVideoCapture) {
            if (hasVideoCapture) {
              internalStopVideoCapture();
            }
          }
        }
        
        {
          AutoRecursiveLock lock(mLifetimeLock);
          
          mLifetimeHasAudio = wantAudio;
          mLifetimeHasVideoCapture = wantVideoCapture;
          mLifetimeHasVideoChannel = wantVideoChannel;
          mLifetimeHasRecordVideoCapture = wantRecordVideoCapture;
          
          mLifetimeInProgress = false;
        }
        
        if (repeat) {
          ZS_LOG_DEBUG(log("repeating media thread operation again"))
          (*this)();
          return;
        }
        
        ZS_LOG_DEBUG(log("media engine lifetime thread completed"))
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStartVoice()
      {
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("start voice"))
          
          mVoiceChannel = mVoiceBase->CreateChannel();
          if (mVoiceChannel < 0) {
            ZS_LOG_ERROR(Detail, log("could not create voice channel (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            mVoiceChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
            return;
          }
          
          mError = registerVoiceTransport();
          if (mError != 0)
            return;
          
          webrtc::EcModes ecMode = getEcMode();
          if (ecMode == webrtc::kEcUnchanged) {
            return;
          }
          mError = mVoiceAudioProcessing->SetEcStatus(mEcEnabled, ecMode);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller status (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          if (ecMode == webrtc::kEcAecm && mEcEnabled) {
            mError = mVoiceAudioProcessing->SetAecmMode(webrtc::kAecmSpeakerphone);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set acoustic echo canceller mobile mode (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
          mError = mVoiceAudioProcessing->SetAgcStatus(mAgcEnabled, webrtc::kAgcAdaptiveDigital);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set automatic gain control status (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVoiceAudioProcessing->SetNsStatus(mNsEnabled, webrtc::kNsLowSuppression);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set noise suppression status (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVoiceVolumeControl->SetInputMute(-1, false);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set microphone mute (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
#ifdef TARGET_OS_IPHONE
          mError = mVoiceHardware->SetLoudspeakerStatus(false);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set loudspeaker (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
#endif
          webrtc::CodecInst cinst;
          memset(&cinst, 0, sizeof(webrtc::CodecInst));
          for (int idx = 0; idx < mVoiceCodec->NumOfCodecs(); idx++) {
            mError = mVoiceCodec->GetCodec(idx, cinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to get voice codec (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
#ifdef OPENPEER_MEDIA_ENGINE_VOICE_CODEC_ISAC
            if (strcmp(cinst.plname, "ISAC") == 0) {
              strcpy(cinst.plname, "ISAC");
              cinst.pltype = 103;
              cinst.rate = 32000;
              cinst.pacsize = 480; // 30ms
              cinst.plfreq = 16000;
              cinst.channels = 1;
              mError = mVoiceCodec->SetSendCodec(mVoiceChannel, cinst);
              if (mError != 0) {
                ZS_LOG_ERROR(Detail, log("failed to set send voice codec (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
                return;
              }
              break;
            }
#elif defined OPENPEER_MEDIA_ENGINE_VOICE_CODEC_OPUS
            if (strcmp(cinst.plname, "OPUS") == 0) {
              strcpy(cinst.plname, "OPUS");
              cinst.pltype = 110;
              cinst.rate = 20000;
              cinst.pacsize = 320; // 20ms
              cinst.plfreq = 16000;
              cinst.channels = 1;
              mError = mVoiceCodec->SetSendCodec(mVoiceChannel, cinst);
              if (mError != 0) {
                ZS_LOG_ERROR(Detail, log("failed to set send voice codec (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
                return;
              }
              break;
            }
#endif
          }
          
          webrtc::CodecInst cfinst;
          memset(&cfinst, 0, sizeof(webrtc::CodecInst));
          for (int idx = 0; idx < mVoiceCodec->NumOfCodecs(); idx++) {
            mError = mVoiceCodec->GetCodec(idx, cfinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to get voice codec (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
            if (strcmp(cfinst.plname, "VORBIS") == 0) {
              strcpy(cfinst.plname, "VORBIS");
              cfinst.pltype = 109;
              cfinst.rate = 32000;
              cfinst.pacsize = 480; // 30ms
              cfinst.plfreq = 16000;
              cfinst.channels = 1;
              break;
            }
          }
          
          mError = setVoiceTransportParameters();
          if (mError != 0)
            return;
          
          mError = mVoiceBase->StartSend(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start sending voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVoiceBase->StartReceive(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start receiving voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVoiceBase->StartPlayout(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start playout (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          if (!mVoiceRecordFile.empty()) {
            mError = mVoiceFile->StartRecordingCall(mVoiceRecordFile, &cfinst);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to start call recording (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            }
          }
        }
        
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVoiceEngineReady = true;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStopVoice()
      {
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVoiceEngineReady = false;
        }
        
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("stop voice"))
          
          mError = mVoiceBase->StopSend(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop sending voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVoiceBase->StopPlayout(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop playout (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVoiceBase->StopReceive(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop receiving voice (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          if (!mVoiceRecordFile.empty()) {
            mError = mVoiceFile->StopRecordingCall();
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to stop call recording (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            }
            mVoiceRecordFile.erase();
          }
          mError = mVoiceNetwork->DeRegisterExternalTransport(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to deregister voice external transport (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mError = mVoiceBase->DeleteChannel(mVoiceChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to delete voice channel (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          mVoiceChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        }
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::registerVoiceTransport()
      {
        if (NULL != mVoiceTransport) {
          mError = mVoiceNetwork->RegisterExternalTransport(mVoiceChannel, *mVoiceTransport);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to register voice external transport (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return mError;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("external voice transport is not set"))
          return -1;
        }
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::setVoiceTransportParameters()
      {
        // No transport parameters for external transport.
        return 0;
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStartVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("start video capture - camera type: ") + (mCameraType == CameraType_Back ? "back" : "front"))
          
          const unsigned int KMaxDeviceNameLength = 128;
          const unsigned int KMaxUniqueIdLength = 256;
          char deviceName[KMaxDeviceNameLength];
          memset(deviceName, 0, KMaxDeviceNameLength);
          char uniqueId[KMaxUniqueIdLength];
          memset(uniqueId, 0, KMaxUniqueIdLength);
          uint32_t captureIdx;
          
          if (mCameraType == CameraType_Back)
          {
            captureIdx = 0;
          }
          else if (mCameraType == CameraType_Front)
          {
            captureIdx = 1;
          }
          else
          {
            ZS_LOG_ERROR(Detail, log("camera type is not set"))
            return;
          }
          
#if defined(TARGET_OS_IPHONE) || defined(__QNX__)
          void *captureView = mCaptureRenderView;
#else
          void *captureView = NULL;
#endif
#ifndef __QNX__
          if (captureView == NULL) {
            ZS_LOG_ERROR(Detail, log("capture view is not set"))
            return;
          }
#endif
          
          webrtc::VideoCaptureModule::DeviceInfo *devInfo = webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
          if (devInfo == NULL) {
            ZS_LOG_ERROR(Detail, log("failed to create video capture device info"))
            return;
          }
          
          mError = devInfo->GetDeviceName(captureIdx, deviceName,
                                          KMaxDeviceNameLength, uniqueId,
                                          KMaxUniqueIdLength);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get video device name"))
            return;
          }
          
          strcpy(mDeviceUniqueId, uniqueId);
          
          mVcpm = webrtc::VideoCaptureFactory::Create(1, uniqueId);
          if (mVcpm == NULL) {
            ZS_LOG_ERROR(Detail, log("failed to create video capture module"))
            return;
          }
          
          mError = mVideoCapture->AllocateCaptureDevice(*mVcpm, mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to allocate video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mVcpm->AddRef();
          delete devInfo;
          
          mError = mVideoCapture->RegisterObserver(mCaptureId, *this);
          if (mError < 0) {
            ZS_LOG_ERROR(Detail, log("failed to register video capture observer (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
#ifdef TARGET_OS_IPHONE
          webrtc::CapturedFrameOrientation defaultOrientation;
          switch (mDefaultVideoOrientation) {
            case IMediaEngineObsolete::VideoOrientation_LandscapeLeft:
              defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
              break;
            case IMediaEngineObsolete::VideoOrientation_PortraitUpsideDown:
              defaultOrientation = webrtc::CapturedFrameOrientation_PortraitUpsideDown;
              break;
            case IMediaEngineObsolete::VideoOrientation_LandscapeRight:
              defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeRight;
              break;
            case IMediaEngineObsolete::VideoOrientation_Portrait:
              defaultOrientation = webrtc::CapturedFrameOrientation_Portrait;
              break;
            default:
              defaultOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
              break;
          }
          mError = mVideoCapture->SetDefaultCapturedFrameOrientation(mCaptureId, defaultOrientation);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to set default orientation on video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          setVideoCaptureRotation();
          
          webrtc::RotateCapturedFrame orientation;
          mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
#else
          webrtc::RotateCapturedFrame orientation = webrtc::RotateCapturedFrame_0;
#endif
          
          int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
          mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
          if (mError != 0)
            return;
          
          webrtc::CaptureCapability capability;
          capability.width = width;
          capability.height = height;
          capability.maxFPS = maxFramerate;
          capability.rawType = webrtc::kVideoI420;
          capability.faceDetection = mFaceDetection;
          mError = mVideoCapture->StartCapture(mCaptureId, capability);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start capturing (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
#ifndef __QNX__
          mError = mVideoRender->AddRenderer(mCaptureId, captureView, 0, 0.0F, 0.0F, 1.0F,
                                             1.0F);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to add renderer for video capture (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVideoRender->StartRender(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start rendering video capture (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
#endif
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStopVideoCapture()
      {
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("stop video capture"))
          
#ifndef __QNX__
          mError = mVideoRender->StopRender(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop rendering video capture (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoRender->RemoveRenderer(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to remove renderer for video capture (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
#endif
          mError = mVideoCapture->StopCapture(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop video capturing (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoCapture->ReleaseCaptureDevice(mCaptureId);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to release video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          if (mVcpm != NULL)
            mVcpm->Release();
          
          mVcpm = NULL;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStartVideoChannel()
      {
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("start video channel"))
          
#if defined(TARGET_OS_IPHONE) || defined(__QNX__)
          void *channelView = mChannelRenderView;
#else
          void *channelView = NULL;
#endif
          if (channelView == NULL) {
            ZS_LOG_ERROR(Detail, log("channel view is not set"))
            return;
          }
          
          mError = mVideoBase->CreateChannel(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("could not create video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = registerVideoTransport();
          if (0 != mError)
            return;
          
          mError = mVideoNetwork->SetMTU(mVideoChannel, mMtu);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set MTU for video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVideoCapture->ConnectCaptureDevice(mCaptureId, mVideoChannel);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to connect capture device to video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVideoRtpRtcp->SetRTCPStatus(mVideoChannel, webrtc::kRtcpCompound_RFC4585);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set video RTCP status (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVideoRtpRtcp->SetKeyFrameRequestMethod(mVideoChannel,
                                                           webrtc::kViEKeyFrameRequestPliRtcp);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set key frame request method (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVideoRtpRtcp->SetTMMBRStatus(mVideoChannel, true);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to set temporary max media bit rate status (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
#ifdef TARGET_OS_IPHONE
          OutputAudioRoute route;
          mError = mVoiceHardware->GetOutputAudioRoute(route);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to get output audio route (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
            return;
          }
          if (route != webrtc::kOutputAudioRouteHeadphone)
          {
            mError = mVoiceHardware->SetLoudspeakerStatus(true);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to set loudspeaker (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
              return;
            }
          }
#endif
          
          mError = mVideoRender->AddRenderer(mVideoChannel, channelView, 0, 0.0F, 0.0F, 1.0F,
                                             1.0F);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to add renderer for video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          webrtc::VideoCodec videoCodec;
          memset(&videoCodec, 0, sizeof(VideoCodec));
          for (int idx = 0; idx < mVideoCodec->NumberOfCodecs(); idx++) {
            mError = mVideoCodec->GetCodec(idx, videoCodec);
            if (mError != 0) {
              ZS_LOG_ERROR(Detail, log("failed to get video codec (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
              return;
            }
            if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
              mError = mVideoCodec->SetSendCodec(mVideoChannel, videoCodec);
              if (mError != 0) {
                ZS_LOG_ERROR(Detail, log("failed to set send video codec (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
                return;
              }
              break;
            }
          }
          
          mError = setVideoCodecParameters();
          if (mError != 0) {
            return;
          }
          
          mError = setVideoTransportParameters();
          if (mError != 0)
            return;
          
          mError = mVideoBase->StartSend(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start sending video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mError = mVideoBase->StartReceive(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start receiving video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoRender->StartRender(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to start rendering video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
        }
        
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVideoEngineReady = true;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStopVideoChannel()
      {
        {
          AutoRecursiveLock lock(mMediaEngineReadyLock);
          mVideoEngineReady = false;
        }
        
        {
          AutoRecursiveLock lock(mLock);
          
          ZS_LOG_DEBUG(log("stop video channel"))
          
          mError = mVideoRender->StopRender(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop rendering video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoRender->RemoveRenderer(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to remove renderer for video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoBase->StopSend(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop sending video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoBase->StopReceive(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to stop receiving video (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = mVideoCapture->DisconnectCaptureDevice(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to disconnect capture device from video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          mError = deregisterVideoTransport();
          if (0 != mError)
            return;
          mError = mVideoBase->DeleteChannel(mVideoChannel);
          if (mError != 0) {
            ZS_LOG_ERROR(Detail, log("failed to delete video channel (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return;
          }
          
          mVideoChannel = OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStartRecordVideoCapture(String videoRecordFile, bool saveVideoToLibrary)
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("start video capture recording"))
        
        webrtc::CapturedFrameOrientation recordOrientation;
        switch (mRecordVideoOrientation) {
          case IMediaEngineObsolete::VideoOrientation_LandscapeLeft:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
          case IMediaEngineObsolete::VideoOrientation_PortraitUpsideDown:
            recordOrientation = webrtc::CapturedFrameOrientation_PortraitUpsideDown;
            break;
          case IMediaEngineObsolete::VideoOrientation_LandscapeRight:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeRight;
            break;
          case IMediaEngineObsolete::VideoOrientation_Portrait:
            recordOrientation = webrtc::CapturedFrameOrientation_Portrait;
            break;
          default:
            recordOrientation = webrtc::CapturedFrameOrientation_LandscapeLeft;
            break;
        }
        mError = mVideoCapture->SetCapturedFrameLockedOrientation(mCaptureId, recordOrientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set record orientation on video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        }
        mError = mVideoCapture->EnableCapturedFrameOrientationLock(mCaptureId, true);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to enable orientation lock on video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        }
        
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        }
        
        if (mVideoChannel == OPENPEER_MEDIA_ENGINE_INVALID_CHANNEL)
          setVideoCaptureRotation();
        else
          setVideoCodecParameters();
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return;
        
        webrtc::CodecInst audioCodec;
        memset(&audioCodec, 0, sizeof(webrtc::CodecInst));
        strcpy(audioCodec.plname, "AAC");
        audioCodec.rate = 32000;
        audioCodec.plfreq = 16000;
        audioCodec.channels = 1;
        
        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        videoCodec.codecType = webrtc::kVideoCodecH264;
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = maxFramerate;
        videoCodec.maxBitrate = maxBitrate;
        
        mError = mVideoFile->StartRecordCaptureVideo(mCaptureId, videoRecordFile, webrtc::MICROPHONE, audioCodec, videoCodec, webrtc::kFileFormatMP4File, saveVideoToLibrary);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to start video capture recording (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
      }
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::internalStopRecordVideoCapture()
      {
        AutoRecursiveLock lock(mLock);
        
        ZS_LOG_DEBUG(log("stop video capture recording"))
        
        mError = mVideoFile->StopRecordCaptureVideo(mCaptureId);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to stop video capture recording (error: ") + Stringize<INT>(mVoiceBase->LastError()).string() + ")")
          return;
        }
        
        mError = mVideoCapture->EnableCapturedFrameOrientationLock(mCaptureId, false);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to disable orientation lock on video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return;
        }
        
        try {
          if (mDelegate)
            mDelegate->onMediaEngineVideoCaptureRecordStopped();
        } catch (IMediaEngineDelegateObsoleteProxy::Exceptions::DelegateGone &) {
          ZS_LOG_WARNING(Detail, log("delegate gone"))
        }
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::registerVideoTransport()
      {
        if (NULL != mVideoTransport) {
          mError = mVideoNetwork->RegisterSendTransport(mVideoChannel, *mVideoTransport);
          if (0 != mError) {
            ZS_LOG_ERROR(Detail, log("failed to register video external transport (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
            return mError;
          }
        } else {
          ZS_LOG_ERROR(Detail, log("external video transport is not set"))
          return -1;
        }
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::deregisterVideoTransport()
      {
        mError = mVideoNetwork->DeregisterSendTransport(mVideoChannel);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to deregister video external transport (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::setVideoTransportParameters()
      {
        // No transport parameters for external transport.
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::getVideoCaptureParameters(webrtc::RotateCapturedFrame orientation, int& width, int& height, int& maxFramerate, int& maxBitrate)
      {
#ifdef TARGET_OS_IPHONE
        String iPadString("iPad");
        String iPad2String("iPad2");
        String iPadMiniString("iPad2,5");
        String iPad3String("iPad3");
        String iPad4String("iPad3,4");
        String iPhoneString("iPhone");
        String iPhone4SString("iPhone4,1");
        String iPhone5String("iPhone5");
        String iPodString("iPod");
        String iPod4String("iPod4,1");
        if (mCameraType == CameraType_Back) {
          if (orientation == webrtc::RotateCapturedFrame_0 || orientation == webrtc::RotateCapturedFrame_180) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 160;
              height = 90;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 480;
              height = 270;
              maxFramerate = 15;
              maxBitrate = 300;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 160;
              height = 90;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 480;
              height = 270;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          } else if (orientation == webrtc::RotateCapturedFrame_90 || orientation == webrtc::RotateCapturedFrame_270) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 90;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 270;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 300;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 90;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 270;
              height = 480;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          }
        } else if (mCameraType == CameraType_Front) {
          if (orientation == webrtc::RotateCapturedFrame_0 || orientation == webrtc::RotateCapturedFrame_180) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 160;
              height = 120;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 160;
              height = 120;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPad4String.size(), iPad4String) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 320;
              height = 180;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
              width = 320;
              height = 240;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          } else if (orientation == webrtc::RotateCapturedFrame_90 || orientation == webrtc::RotateCapturedFrame_270) {
            if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
              width = 120;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
              width = 120;
              height = 160;
              maxFramerate = 5;
              maxBitrate = 100;
            } else if (mMachineName.compare(0, iPad4String.size(), iPad4String) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadMiniString.size(), iPadMiniString) >= 0) {
              width = 180;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
              width = 240;
              height = 320;
              maxFramerate = 15;
              maxBitrate = 250;
            } else {
              ZS_LOG_ERROR(Detail, log("machine name is not supported"))
              return -1;
            }
          }
        } else {
          ZS_LOG_ERROR(Detail, log("camera type is not set"))
          return -1;
        }
#else
        width = 180;
        height = 320;
        maxFramerate = 15;
        maxBitrate = 250;
#endif
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::setVideoCaptureRotation()
      {
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        mError = mVideoCapture->SetRotateCapturedFrames(mCaptureId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set rotation for video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        
        const char *rotationString = NULL;
        switch (orientation) {
          case webrtc::RotateCapturedFrame_0:
            rotationString = "0 degrees";
            break;
          case webrtc::RotateCapturedFrame_90:
            rotationString = "90 degrees";
            break;
          case webrtc::RotateCapturedFrame_180:
            rotationString = "180 degrees";
            break;
          case webrtc::RotateCapturedFrame_270:
            rotationString = "270 degrees";
            break;
          default:
            break;
        }
        
        if (rotationString) {
          ZS_LOG_DEBUG(log("video capture rotation set - rotation: ") + rotationString)
        }
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::setVideoCodecParameters()
      {
#ifdef TARGET_OS_IPHONE
        webrtc::RotateCapturedFrame orientation;
        mError = mVideoCapture->GetOrientation(mDeviceUniqueId, orientation);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get orientation from video capture device (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
#else
        webrtc::RotateCapturedFrame orientation = webrtc::RotateCapturedFrame_0;
#endif
        
        int width = 0, height = 0, maxFramerate = 0, maxBitrate = 0;
        mError = getVideoCaptureParameters(orientation, width, height, maxFramerate, maxBitrate);
        if (mError != 0)
          return mError;
        
        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(VideoCodec));
        mError = mVideoCodec->GetSendCodec(mVideoChannel, videoCodec);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to get video codec (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = maxFramerate;
        videoCodec.maxBitrate = maxBitrate;
        mError = mVideoCodec->SetSendCodec(mVideoChannel, videoCodec);
        if (mError != 0) {
          ZS_LOG_ERROR(Detail, log("failed to set send video codec (error: ") + Stringize<INT>(mVideoBase->LastError()).string() + ")")
          return mError;
        }
        
        ZS_LOG_DEBUG(log("video codec size - width: ") + Stringize<INT>(width).string() + ", height: " + Stringize<INT>(height).string())
        
        return 0;
      }
      
      //-----------------------------------------------------------------------
      webrtc::EcModes MediaEngineObsolete::getEcMode()
      {
#ifdef TARGET_OS_IPHONE
        String iPadString("iPad");
        String iPad2String("iPad2");
        String iPad3String("iPad3");
        String iPhoneString("iPhone");
        String iPhone5String("iPhone5");
        String iPhone4SString("iPhone4,1");
        String iPodString("iPod");
        String iPod4String("iPod4,1");
        
        if (mMachineName.compare(0, iPod4String.size(), iPod4String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPodString.size(), iPodString) >= 0) {
          return webrtc::kEcAecm;
        } else if (mMachineName.compare(0, iPhone5String.size(), iPhone5String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPhone4SString.size(), iPhone4SString) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPhoneString.size(), iPhoneString) >= 0) {
          return webrtc::kEcAecm;
        } else if (mMachineName.compare(0, iPad3String.size(), iPad3String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPad2String.size(), iPad2String) >= 0) {
          return webrtc::kEcAec;
        } else if (mMachineName.compare(0, iPadString.size(), iPadString) >= 0) {
          return webrtc::kEcAecm;
        } else {
          ZS_LOG_ERROR(Detail, log("machine name is not supported"))
          return webrtc::kEcUnchanged;
        }
#elif defined(__QNX__)
        return webrtc::kEcAec;
#else
        return webrtc::kEcUnchanged;
#endif
      }
      
      //-----------------------------------------------------------------------
      String MediaEngineObsolete::log(const char *message) const
      {
        return String("MediaEngineObsolete [") + Stringize<typeof(mID)>(mID).string() + "] " + message;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete::RedirectTransport
#pragma mark
      
      //-----------------------------------------------------------------------
      MediaEngineObsolete::RedirectTransport::RedirectTransport(const char *transportType) :
      mID(zsLib::createPUID()),
      mTransportType(transportType),
      mTransport(0)
      {
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete::RedirectTransport => webrtc::Transport
#pragma mark
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::RedirectTransport::SendPacket(int channel, const void *data, int len)
      {
        Transport *transport = NULL;
        {
          AutoRecursiveLock lock(mLock);
          transport = mTransport;
        }
        if (!transport) {
          ZS_LOG_WARNING(Debug, log("RTP packet cannot be sent as no transport is not registered") + ", channel=" + Stringize<typeof(channel)>(channel).string() + ", length=" + Stringize<typeof(len)>(len).string())
          return 0;
        }
        
        return transport->SendPacket(channel, data, len);
      }
      
      //-----------------------------------------------------------------------
      int MediaEngineObsolete::RedirectTransport::SendRTCPPacket(int channel, const void *data, int len)
      {
        Transport *transport = NULL;
        {
          AutoRecursiveLock lock(mLock);
          transport = mTransport;
        }
        if (!transport) {
          ZS_LOG_WARNING(Debug, log("RTCP packet cannot be sent as no transport is not registered") + ", channel=" + Stringize<typeof(channel)>(channel).string() + ", length=" + Stringize<typeof(len)>(len).string())
          return 0;
        }
        
        return transport->SendRTCPPacket(channel, data, len);
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete::RedirectTransport => friend MediaEngineObsolete
#pragma mark
      
      //-----------------------------------------------------------------------
      void MediaEngineObsolete::RedirectTransport::redirect(Transport *transport)
      {
        AutoRecursiveLock lock(mLock);
        mTransport = transport;
      }
      
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
      //-----------------------------------------------------------------------
#pragma mark
#pragma mark MediaEngineObsolete::RedirectTransport => (internal)
#pragma mark
      
      //-----------------------------------------------------------------------
      String MediaEngineObsolete::RedirectTransport::log(const char *message)
      {
        return String("MediaEngineObsolete::RedirectTransport (") + mTransportType + ") [" + Stringize<typeof(mID)>(mID).string() + "] " + message;
      }
    }
    
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
    //-------------------------------------------------------------------------
#pragma mark
#pragma mark IMediaEngine
#pragma mark
    
    //-------------------------------------------------------------------------
    const char *IMediaEngineObsolete::toString(CameraTypes type)
    {
      switch (type) {
        case CameraType_None:   return "None";
        case CameraType_Front:  return "Front";
        case CameraType_Back:   return "Back";
      }
      return "UNDEFINED";
    }
    
    //---------------------------------------------------------------------------
    const char *IMediaEngineObsolete::toString(VideoOrientations orientation)
    {
      switch (orientation) {
        case VideoOrientation_LandscapeLeft:        return "Landscape left";
        case VideoOrientation_PortraitUpsideDown:   return "Portrait upside down";
        case VideoOrientation_LandscapeRight:       return "Landscape right";
        case VideoOrientation_Portrait:             return "Portrait";
      }
      return "UNDEFINED";
    }
    
    //-------------------------------------------------------------------------
    const char *IMediaEngineObsolete::toString(OutputAudioRoutes route)
    {
      switch (route) {
        case OutputAudioRoute_Headphone:        return "Headphone";
        case OutputAudioRoute_BuiltInReceiver:  return "Built in receiver";
        case OutputAudioRoute_BuiltInSpeaker:   return "Built in speaker";
      }
      return "UNDEFINED";
    }
    
    //-------------------------------------------------------------------------
    IMediaEngineObsoletePtr IMediaEngineObsolete::singleton()
    {
      return internal::MediaEngineObsolete::singleton();
    }
  }
}
