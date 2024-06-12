
#ifndef __binsentry__commercialbin_s3presignedurl_server_glue_h__adaptor__H__
#define __binsentry__commercialbin_s3presignedurl_server_glue_h__adaptor__H__

#include <sdbus-c++/sdbus-c++.h>
#include <string>
#include <tuple>

namespace org {
namespace binsentry {
namespace S3PresignedURL {

class S3PresignedURL_adaptor
{
public:
    static constexpr const char* INTERFACE_NAME = "com.binsentry.CommercialBin.S3PresignedURL.SensorReading";

protected:
    S3PresignedURL_adaptor(sdbus::IObject& object)
        : object_(object)
    {
        object_.registerMethod("RequestS3PreSignedURL").onInterface(INTERFACE_NAME).withOutputParamNames("ReturnCode").withInputParamNames("RequestId").implementedAs([this](const uint16_t requestId){ return this->RequestS3PreSignedURL(requestId); });
        object_.registerSignal("S3PreSignedURLResponse").onInterface(INTERFACE_NAME).withParameters<uint16_t, int32_t, std::string>("RequestId", "ReturnCode", "ResponseJSONString");
        object_.registerProperty("Version").onInterface(INTERFACE_NAME).withGetter([this](){ return this->Version(); });
    }

    ~S3PresignedURL_adaptor() = default;

    void emitS3PreSignedURLResponse(const uint16_t requestId, int32_t returnCode, const std::string& responseJSONString)
    {
        object_.emitSignal("S3PreSignedURLResponse").onInterface(INTERFACE_NAME).withArguments(requestId, returnCode, responseJSONString);
    }

private:
    virtual int32_t RequestS3PreSignedURL(uint16_t requestId) = 0;

private:
    virtual std::uint16_t Version() = 0;

private:
    sdbus::IObject& object_;
};

}}} // namespaces

#endif
