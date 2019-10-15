#include "InferenceMobileSSD.h"
#include <ie_plugin_config.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <inference_engine.hpp>

#include <ext_list.hpp>
#include <ie_plugin_dispatcher.hpp>
#include <ie_plugin_ptr.hpp>
#include <cpp/ie_cnn_net_reader.h>
#include <cpp/ie_infer_request.hpp>
#include <ie_device.hpp>

#include "DataPacket.h"

using namespace std;
using namespace InferenceEngine::details;
using namespace InferenceEngine;

InferenceMobileSSD::InferenceMobileSSD():
    m_inputWidth(0),
    m_inputHeight(0),
    m_channelNum(1),
    m_resultSize(0),
    m_maxResultNum(0)
{
}

InferenceMobileSSD::~InferenceMobileSSD()
{
}

int InferenceMobileSSD::SetDataPorts()
{
	InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
	auto& inputInfoFirst = inputInfo.begin()->second;
	inputInfoFirst->setPrecision(Precision::U8);
	inputInfoFirst->getInputData()->setLayout(Layout::NCHW);

    // ---------------------------Set outputs ------------------------------------------------------	
	InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
	auto& _output = outputInfo.begin()->second;
	_output->setPrecision(Precision::FP32);
	_output->setLayout(Layout::NCHW);	
}

int InferenceMobileSSD::Load(const char *device, const char *model, const char *weights)
{
    int ret = InferenceOV::Load(device, model, weights);
    if (ret)
    {
        return ret;
    }

    // ---------------------------Set outputs ------------------------------------------------------	
    InferenceEngine::OutputsDataMap outputInfo(m_network.getOutputsInfo());
    auto& _output = outputInfo.begin()->second;
    const InferenceEngine::SizeVector outputDims = _output->getTensorDesc().getDims();
    m_maxResultNum= outputDims[2];
    m_resultSize = outputDims[3];

    InferenceEngine::InputsDataMap inputInfo(m_network.getInputsInfo());
    auto& inputInfoFirst = inputInfo.begin()->second;
    const InferenceEngine::SizeVector inputDims = inputInfoFirst->getTensorDesc().getDims();
    m_channelNum = inputDims[1];
    m_inputWidth = inputDims[3];
    m_inputHeight = inputDims[2];

    return 0;
}

void InferenceMobileSSD::CopyImage(const uint8_t *img, void *dst, uint32_t batchIndex)
{
    uint8_t *input = (uint8_t *)dst;
    input += batchIndex * m_inputWidth * m_inputHeight * m_channelNum;
    //
    // The img in BGR format
    //cv::Mat imgMat(m_inputHeight, m_inputWidth, CV_8UC1, img);
    //std::vector<cv::Mat> inputChannels;
    //for (int i = 0; i < m_channelNum; i ++)
    //{
    //    cv::Mat channel(m_inputHeight, m_inputWidth, CV_8UC1, input + i * m_inputWidth * m_inputHeight);
    //    inputChannels.push_back(channel);
    //}
    //cv::split(imgMat, inputChannels);

    //
    // The img should already in RGBP format, if not, using the above code
    memcpy(input, img, m_channelNum * m_inputWidth * m_inputHeight);
}

int InferenceMobileSSD::Translate(std::vector<VAData *> &datas, uint32_t count, void *result, uint32_t *channelIds, uint32_t *frameIds)
{
    float *curResult = (float *)result;

    typedef std::vector<VAData *> VADataVector;
    VADataVector *tempBuffer = new VADataVector[count];
    
    for (int i = 0; i < m_maxResultNum; i ++)
    {
        int imgid = (int)curResult[0];
        if (imgid < 0 || curResult[2] == 0 || imgid > count)
        {
            break;
        }
        int c = (int)curResult[1];
        float conf = curResult[2];
        int x = (int)(curResult[3] * m_inputWidth);
        int y = (int)(curResult[4] * m_inputHeight);
        int r = (int)(curResult[5] * m_inputWidth);
        int b = (int)(curResult[6] * m_inputHeight);
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (r >= m_inputWidth) r = m_inputWidth - 1;
        if (b >= m_inputHeight) b = m_inputHeight - 1;
        VAData *data = VAData::Create(x, y, r-x, b-y, c, conf);
        data->SetID(channelIds[imgid], frameIds[imgid]);
        tempBuffer[imgid].push_back(data);
        curResult += m_resultSize;
    }

    for (int i = 0; i < count; i++)
    {
        datas.insert(datas.end(), tempBuffer[i].begin(), tempBuffer[i].end());
    }

    delete[] tempBuffer;
    return 0;
}
