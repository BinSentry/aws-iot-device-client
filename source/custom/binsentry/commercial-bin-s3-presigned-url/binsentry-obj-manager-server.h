//
// Created by mvermeer on 16/05/24.
//

#ifndef AWS_IOT_DEVICE_CLIENT_BINSENTRY_OBJ_MANAGER_SERVER_H
#define AWS_IOT_DEVICE_CLIENT_BINSENTRY_OBJ_MANAGER_SERVER_H

#include "binsentry-s3-presigned-url-server-glue.h"
#include <chrono>
#include <iostream>
#include <memory>
#include <sdbus-c++/sdbus-c++.h>
#include <thread>

class ManagerAdaptor : public sdbus::AdaptorInterfaces< sdbus::ObjectManager_adaptor >
{
  public:
    ManagerAdaptor(sdbus::IConnection& connection, std::string path)
        : AdaptorInterfaces(connection, std::move(path))
    {
        registerAdaptor();
    }

    ~ManagerAdaptor()
    {
        unregisterAdaptor();
    }
};

class S3PresignedURLAdaptor final : public sdbus::AdaptorInterfaces< org::binsentry::S3PresignedURL::S3PresignedURL_adaptor,
                                                                    sdbus::ManagedObject_adaptor,
                                                                    sdbus::Properties_adaptor >
{
  public:
    /**
     * Invoked when an S3 presigned URL request occurs on the D-Bus.
     *
     * @param requestId The request ID value to use for the S3 presigned URL request.
     */
    using S3PresignedURLRequestHandler =
        std::function<int32_t(uint16_t requestId)>;

    S3PresignedURLAdaptor(sdbus::IConnection& connection, std::string path, std::string name,
                          S3PresignedURLRequestHandler &&s3PresignedURLRequestHandler)
        : AdaptorInterfaces(connection, std::move(path))
          , m_name(std::move(name))
          , m_requestHandler(std::move(s3PresignedURLRequestHandler))
    {
        registerAdaptor();
        emitInterfacesAddedSignal({org::binsentry::S3PresignedURL::S3PresignedURL_adaptor::INTERFACE_NAME});
    }

    ~S3PresignedURLAdaptor()
    {
        emitInterfacesRemovedSignal({org::binsentry::S3PresignedURL::S3PresignedURL_adaptor::INTERFACE_NAME});
        unregisterAdaptor();
    }

    int32_t RequestS3PreSignedURL(uint16_t requestId) override
    {
        if (m_requestHandler != nullptr) {
            return m_requestHandler(requestId);
        } else {
            return -1;
        }
    }

    using S3PresignedURL_adaptor::emitS3PreSignedURLResponse;

    std::uint16_t Version() override
    {
        return m_version;
    }

  private:
    std::string m_name;
    std::uint16_t m_version = 1;
    S3PresignedURLRequestHandler m_requestHandler = nullptr;
};

#endif // AWS_IOT_DEVICE_CLIENT_BINSENTRY_OBJ_MANAGER_SERVER_H
