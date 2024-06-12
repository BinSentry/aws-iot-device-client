#ifndef DEVICE_CLIENT_BINSENTRYCOMMBINS3PRESIGNEDURLFEATURE_H
#define DEVICE_CLIENT_BINSENTRYCOMMBINS3PRESIGNEDURLFEATURE_H

#include <aws/iot/MqttClient.h>
#include <sdbus-c++/IConnection.h>

#include "../../../ClientBaseNotifier.h"
#include "../../../Feature.h"
#include "../../../SharedCrtResourceManager.h"
#include "../../../config/Config.h"
#include "../../../util/FileUtils.h"
#include "binsentry-obj-manager-server.h"

namespace Aws
{
    namespace Iot
    {
        namespace DeviceClient
        {
            namespace Custom
            {
                namespace Binsentry
                {
                    /**
                     * \brief Provides IoT BinSentry Commercial Bin S3 Presigned URL retrieval functionality within the
                     * Device Client. When enabled the feature will create a D-Bus interface for request/response
                     * inter-process communication.
                     * Behind the scenes the Device Client MQTT connect is used to obtain the S3 pre-signed URL.
                     */
                    class S3PresignedURLFeature : public Feature
                    {
                      public:
                        static constexpr char NAME[] = "BinSentry Custom S3 Presigned URL";
                        bool createPubSub(
                            const PlainConfig &config,
                            const std::string &absFilePath,
                            const aws_byte_buf *payload) const;
                        /**
                         * \brief Initializes the PubSub feature with all the required setup information, event
                         * handlers, and the SharedCrtResourceManager
                         *
                         * @param manager The resource manager used to manage CRT resources
                         * @param notifier an ClientBaseNotifier used for notifying the client base of events or errors
                         * @param config configuration information passed in by the user via either the command line or
                         * configuration file
                         * @return a non-zero return code indicates a problem. The logs can be checked for more info
                         */
                        int init(
                            std::shared_ptr<SharedCrtResourceManager> manager,
                            std::shared_ptr<ClientBaseNotifier> notifier,
                            const PlainConfig &config);
                        void LoadFromConfig(const PlainConfig &config);

                        // Interface methods defined in Feature.h
                        std::string getName() override;
                        int start() override;
                        int stop() override;

                      private:
                        /**
                         * \brief the ThingName to use
                         */
                        std::string thingName;
                        static constexpr char TAG[] = "custom/BinsentryCommBinS3PresignedURLFeature.cpp";

                        int publishS3PresignedURLRequest(unsigned int requestId, int64_t timeout_ms);
                        void subscribeToS3PresignedURLResponse();
                        int subscribeToS3PresignedURLResponse(int64_t timeout_ms);
                        int subscribeToS3PresignedURLResponse(int64_t timeout_ms, int retries);
                        void onSubscribeReceiveS3PresignedURLResponse(
                            const Crt::Mqtt::MqttConnection &connection, const Crt::String &topic,
                            const Crt::ByteBuf &payload, bool dup, Crt::Mqtt::QOS qos, bool retain);
                        mutable std::mutex subscribeLock;
                        std::atomic<bool> hasSubscribedToS3PresignedURLResponse{false};

                        /**
                         * \brief The resource manager used to manage CRT resources
                         */
                        std::shared_ptr<SharedCrtResourceManager> resourceManager;
                        /**
                         * \brief An interface used to notify the Client base if there is an event that requires its
                         * attention
                         */
                        std::shared_ptr<ClientBaseNotifier> baseNotifier;
                        /**
                         * \brief Whether the DeviceClient base has requested this feature to stop.
                         */
                        std::atomic<bool> needStop{false};
                        /**
                         * \brief Topic for publishing data to
                         */
                        std::string pubTopic;
                        /**
                         * \brief Topic to subscribe to
                         */
                        std::string subTopic;

                        static constexpr char DBUS_BUS_NAME[] = "com.binsentry.CommercialBin";
                        static constexpr char DBUS_PATH_BASE_NAME[] = "/com/binsentry/CommercialBin";
                        static constexpr char DBUS_PATH_NAME[] = "/com/binsentry/CommercialBin/S3PresignedURL/SensorReadingHDF5";
                        std::unique_ptr<sdbus::IConnection> dbusConnection = nullptr;
                        std::unique_ptr<ManagerAdaptor> dbusManager = nullptr;
                        std::unique_ptr<S3PresignedURLAdaptor> dbusS3PresignedURL = nullptr;
                        mutable std::mutex dbusLock;

                        void setupDBus();
                        void cleanupDBus();
                        bool isDBusSetup();
                        bool checkIfSetupOrTryDBusSetup();
                    };
                } // namespace Binsentry
            } // namespace Samples
        } // namespace DeviceClient
    } // namespace Iot
} // namespace Aws

#endif // DEVICE_CLIENT_BINSENTRYCOMMBINS3PRESIGNEDURLFEATURE_H
