#include <iostream>

extern "C"
{
#include "libavdevice/avdevice.h"
#include "libavutil/imgutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/timestamp.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}

int frameCount;
int nbFrameCount;
uint8_t *videoDstData[4];
int linesizes[4];
AVPixelFormat videoPixelFormat;
int videoBufferSize = 0;
FILE *dstfile = nullptr;

int DecodePacket(AVCodecContext *dec, const AVPacket *pkt , AVFrame *frame){
     int ret = 0;

    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        return ret;
    }

    // std::cout << "avcodec_send_packet ret = " << ret << std::endl;

    // get all the available frames from the decoder
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
       //  std::cout << "avcodec_receive_frame ret = " << ret << std::endl;
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                return 0;
            return ret;
        }

        // write the frame data to output file
        if (dec->codec->type == AVMEDIA_TYPE_VIDEO){
            av_image_copy(videoDstData , linesizes 
                ,  const_cast<const uint8_t **>(frame->data)
                , frame->linesize 
                , AVPixelFormat::AV_PIX_FMT_YUV420P
                , frame->width , frame->height);

            frameCount++;
            
            fwrite(videoDstData[0], 1, videoBufferSize, dstfile);
            std::cout << frameCount << " / " << nbFrameCount << std::endl;


            // std::cout 
            //     << "frame width :" << frame->width 
            //     << "  height : " << frame->height 
            //     <<"  fmt:" << dec->pix_fmt 
            //     << " keyframe = " << frame->key_frame << std::endl;
        }

        av_frame_unref(frame);
        if (ret < 0)
            return ret;
    }//end while
    return 0;
}

int main(){
    // avdevice_register_all();
    // avformat_network_init();

    AVFormatContext *formatContext = nullptr;
    // const char *path = "e:/assets/mv/wanzi.mp4";
     const char *path = "d:/test.gif";
    
    int ret = avformat_open_input(&formatContext , path , nullptr , nullptr);

    if(ret == 0){
        std::cout << "open file " << path << " success " <<std::endl;
    }else{
        std::cout << "open failed!! "<< std::endl;
        avformat_network_deinit();
        return -1;
    }

    ret = avformat_find_stream_info(formatContext , nullptr);
    if(ret < 0){
        std::cout << "find stream info Error!"<< std::endl;
        avformat_network_deinit();
        return -1;
    }

    int videoStreamIndex = -1;
    for(int i = 0 ; i< formatContext->nb_streams ;i++){
        AVMediaType codecType = formatContext->streams[i]->codecpar->codec_type;
        std::cout << "codecType : " << codecType << std::endl;

        if(codecType == AVMediaType::AVMEDIA_TYPE_VIDEO){
            videoStreamIndex = i;
        }
    }//end for i

    if(videoStreamIndex < 0){
        std::cout << "Not found VideoStream" << std::endl;
        return -1;
    }

    AVCodecParameters *codecParams = formatContext->streams[videoStreamIndex]->codecpar;
    AVCodecID codecId = codecParams->codec_id;
    std::cout << "codecId = " << codecId << std::endl;
    
    const AVCodec *pCodec = avcodec_find_decoder(formatContext->streams[videoStreamIndex]->codecpar->codec_id);

    if(pCodec == nullptr){
        std::cerr << "not found decoder" << std::endl;
        return -1;
    }
    std::cout << "Codec name : " << pCodec->name << std::endl;

    auto videoDuration = formatContext->streams[videoStreamIndex]->duration;
    std::cout << "videoDuration = " << videoDuration << std::endl;
    nbFrameCount = formatContext->streams[videoStreamIndex]->nb_frames;
    std::cout << "nb_frames = " << formatContext->streams[videoStreamIndex]->nb_frames << std::endl;

    AVCodecContext *codecCtx = avcodec_alloc_context3(pCodec);

    avcodec_parameters_to_context(codecCtx , 
        formatContext->streams[videoStreamIndex]->codecpar);

    avcodec_open2(codecCtx , pCodec , nullptr);

    auto videoWidth = codecCtx->width;
    auto videoHeight = codecCtx->height;
    AVPixelFormat pixFmt = codecCtx->pix_fmt;
    videoPixelFormat = codecCtx->pix_fmt;
    
    std::cout << "info : " << videoWidth << " / " << videoHeight << std::endl;
    std::cout << "format : " << codecCtx->pix_fmt << std::endl;

   
    ret = av_image_alloc(videoDstData , linesizes , 
        videoWidth , videoHeight , codecCtx->pix_fmt , 1);
    if(ret < 0){
        std::cerr << "av_image_alloc error" << std::endl;
        return -1;
    }

    videoBufferSize = ret;
    dstfile = fopen("output.yuv" , "wb");

    std::cout << "videoBufferSize = " << videoBufferSize << std::endl;
    std::cout << linesizes[0] <<" ," <<  linesizes[1] << " ,"
        << linesizes[2] <<" ," <<  linesizes[3] << std::endl;

    AVPacket *packet;
    packet = av_packet_alloc();
    if(!packet){
        std::cerr << "alloc packet error" << std::endl;
        return -1;
    }

    AVFrame *frame = av_frame_alloc();
    if (!frame) {
        std::cerr << "alloc frame error" << std::endl;
        return -1;
    }

    std::cout << "start read frame" << std::endl;
    frameCount = 0;
    while (av_read_frame(formatContext, packet) >= 0) {
        if (packet->stream_index == videoStreamIndex){
            ret = DecodePacket(codecCtx, packet , frame);
        }
        av_packet_unref(packet);
        if (ret < 0)
            break;
    }

    /* flush the decoders */
    if (codecCtx){
        DecodePacket(codecCtx, nullptr , frame);
    }

    std::cout << "frameCount : " << frameCount << std::endl;
    std::cout << "Demuxing succeeded" << std::endl;
    
    fclose(dstfile);

    //show ffplay command
    

    av_free(videoDstData[0]);
    avcodec_free_context(&codecCtx);
    av_frame_free(&frame);
    av_packet_free(&packet);
    return 0;
}

// int test1(){
//     std::string congfigure;
//     congfigure = avcodec_configuration();
//     std::cout << congfigure << std::endl;

//     avdevice_register_all();
//     avformat_network_init();

//     AVFormatContext *formatContext = nullptr;
//     const char *path = "e://assets/mv/wanzi.mp4";
//     int ret = avformat_open_input(&formatContext , path , nullptr , nullptr);
    
//     if(ret == 0){
//         std::cout << "open file " << path << " success " <<std::endl;
//     }else{
//         std::cout << "open failed!! "<< std::endl;
//         avformat_network_deinit();
//         return -1;
//     }
    
//     ret = avformat_find_stream_info(formatContext , nullptr);
//     if(ret < 0){
//         std::cout << "find stream info Error!"<< std::endl;
//         avformat_network_deinit();
//         return -1;
//     }

//     int videoStreamIndex = -1;
//     for(int i = 0 ; i< formatContext->nb_streams ;i++){
//         AVMediaType codecType = formatContext->streams[i]->codecpar->codec_type;
//         std::cout << "codecType : " << codecType << std::endl;

//         if(codecType == AVMediaType::AVMEDIA_TYPE_VIDEO){
//             videoStreamIndex = i;
//         }
//     }//end for i

//     if(videoStreamIndex < 0){
//         std::cout << "Not found VideoStream" << std::endl;
//         return -1;
//     }

//     AVCodecParameters *codecParams = formatContext->streams[videoStreamIndex]->codecpar;
//     AVCodecID codecId = codecParams->codec_id;
//     std::cout << "codecId = " << codecId << std::endl;
    
//     auto pDecoder = avcodec_find_decoder(codecId);

//     if(pDecoder == nullptr){
//         std::cerr << "not found decoder" << std::endl;
//         return -1;
//     }

//     auto videoDuration = formatContext->streams[videoStreamIndex]->duration;
//     std::cout << "videoDuration = " << videoDuration << std::endl;
//     std::cout << "nb_frames = " << formatContext->streams[videoStreamIndex]->nb_frames << std::endl;

//     av_dump_format(formatContext , 0 , nullptr , 0);

//     std::cout << "read Videl file" <<std::endl;

//     // AVCodecContext *pCodecContext = formatContext->streams[videoStreamIndex];
//     // avcodec_open2(pCodecContext , pDecoder , nullptr);
//     avformat_network_deinit();

//     return 0;
// }


