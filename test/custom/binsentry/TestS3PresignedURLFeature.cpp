// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: Apache-2.0

#include "../../../source/custom/binsentry/commercial-bin-s3-presigned-url/BinsentryCommBinS3PresignedURLFeature.h"
#include "../../../source/Feature.h"
#include "../../../source/SharedCrtResourceManager.h"
#include "../../../source/ClientBaseNotifier.h"
#include "../../../source/config/Config.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <aws/common/allocator.h>
#include <aws/crt/Api.h>
#include <aws/crt/JsonObject.h>
#include <memory>

using namespace std;
using namespace testing;
using namespace Aws;
using namespace Aws::Crt;
using namespace Aws::Iot::DeviceClient;
using namespace Aws::Iot::DeviceClient::Custom::Binsentry;

class MockNotifier : public ClientBaseNotifier
{
public:
    MOCK_METHOD(
        void,
        onEvent,
        (Feature* feature, ClientBaseEventNotification notification),
        (override));
    
    MOCK_METHOD(
        void,
        onError,
        (Feature* feature, ClientBaseErrorNotification notification, const std::string& message),
        (override));
};

class MockSharedCrtResourceManager : public SharedCrtResourceManager
{
public:
    MOCK_METHOD(aws_allocator*, getAllocator, (), (override));
    MOCK_METHOD(std::shared_ptr<Mqtt::MqttConnection>, getConnection, (), (override));
};

PlainConfig getTestConfig()
{
    constexpr char jsonString[] = R"(
{
    "endpoint": "endpoint value",
    "cert": "/tmp/aws-iot-device-client-test-file",
    "key": "/tmp/aws-iot-device-client-test-file",
    "root-ca": "/tmp/aws-iot-device-client-test-file",
    "thing-name": "test-thing",
    "logging": {
        "level": "ERROR",
        "type": "file",
        "file": "./aws-iot-device-client.log"
    }
})";

    JsonObject jsonObject(jsonString);
    JsonView jsonView = jsonObject.View();

    PlainConfig config;
    config.LoadFromJson(jsonView);

    return config;
}

class TestS3PresignedURLFeature : public ::testing::Test
{
public:
    void SetUp() override
    {
        // Initialize CRT API for tests
        Aws::Crt::ApiHandle apiHandle;
        
        mockNotifier = std::make_shared<MockNotifier>();
        mockResourceManager = std::make_shared<MockSharedCrtResourceManager>();
        
        feature = std::make_unique<S3PresignedURLFeature>();
        
        // Setup default allocator
        allocator = aws_default_allocator();
    }
    
    void TearDown() override
    {
        feature.reset();
    }
    
protected:
    std::unique_ptr<S3PresignedURLFeature> feature;
    std::shared_ptr<MockNotifier> mockNotifier;
    std::shared_ptr<MockSharedCrtResourceManager> mockResourceManager;
    aws_allocator* allocator;
};

// Test getName method
TEST_F(TestS3PresignedURLFeature, GetName)
{
    EXPECT_STREQ(feature->getName().c_str(), "BinSentry Custom S3 Presigned URL");
}

// Test init method with valid config
TEST_F(TestS3PresignedURLFeature, Init_ValidConfig)
{
    PlainConfig config = getTestConfig();
    
    int result = feature->init(mockResourceManager, mockNotifier, config);
    
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test init method with empty config
TEST_F(TestS3PresignedURLFeature, Init_EmptyConfig)
{
    PlainConfig config;
    
    int result = feature->init(mockResourceManager, mockNotifier, config);
    
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test init method with null resource manager
TEST_F(TestS3PresignedURLFeature, Init_NullResourceManager)
{
    PlainConfig config = getTestConfig();
    
    int result = feature->init(nullptr, mockNotifier, config);
    
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test init method with null notifier
TEST_F(TestS3PresignedURLFeature, Init_NullNotifier)
{
    PlainConfig config = getTestConfig();
    
    int result = feature->init(mockResourceManager, nullptr, config);
    
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test start method
TEST_F(TestS3PresignedURLFeature, Start_WithoutInit)
{
    // Test starting without proper initialization
    int result = feature->start();
    
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test stop method
TEST_F(TestS3PresignedURLFeature, Stop_WithoutInit)
{
    // Test stopping without proper initialization
    int result = feature->stop();
    
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test full lifecycle: init -> start -> stop
TEST_F(TestS3PresignedURLFeature, FullLifecycle)
{
    PlainConfig config = getTestConfig();
    
    // Expect onEvent to be called when feature starts
    EXPECT_CALL(*mockNotifier, onEvent(_, ClientBaseEventNotification::FEATURE_STARTED))
        .Times(1);
    
    // Expect onEvent to be called when feature stops
    EXPECT_CALL(*mockNotifier, onEvent(_, ClientBaseEventNotification::FEATURE_STOPPED))
        .Times(1);
    
    // Initialize
    int result = feature->init(mockResourceManager, mockNotifier, config);
    EXPECT_EQ(result, AWS_OP_SUCCESS);
    
    // Start
    result = feature->start();
    EXPECT_EQ(result, AWS_OP_SUCCESS);
    
    // Stop
    result = feature->stop();
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}

// Test multiple init calls
TEST_F(TestS3PresignedURLFeature, MultipleInitCalls)
{
    PlainConfig config = getTestConfig();
    
    // First init
    int result1 = feature->init(mockResourceManager, mockNotifier, config);
    EXPECT_EQ(result1, AWS_OP_SUCCESS);
    
    // Second init should also succeed
    int result2 = feature->init(mockResourceManager, mockNotifier, config);
    EXPECT_EQ(result2, AWS_OP_SUCCESS);
}

// Test that getName returns consistent value
TEST_F(TestS3PresignedURLFeature, GetNameConsistency)
{
    std::string name1 = feature->getName();
    std::string name2 = feature->getName();
    
    EXPECT_EQ(name1, name2);
    EXPECT_EQ(name1, "BinSentry Custom S3 Presigned URL");
}

// Test init method with different thing names
TEST_F(TestS3PresignedURLFeature, Init_DifferentThingNames)
{
    // Test with device with dashes and underscores
    constexpr char jsonString[] = R"(
{
    "endpoint": "endpoint value",
    "cert": "/tmp/aws-iot-device-client-test-file",
    "key": "/tmp/aws-iot-device-client-test-file",
    "root-ca": "/tmp/aws-iot-device-client-test-file",
    "thing-name": "device-with-dashes_and_underscores",
    "logging": {
        "level": "ERROR",
        "type": "file",
        "file": "./aws-iot-device-client.log"
    }
})";

    JsonObject jsonObject(jsonString);
    JsonView jsonView = jsonObject.View();

    PlainConfig config;
    config.LoadFromJson(jsonView);
    
    int result = feature->init(mockResourceManager, mockNotifier, config);
    EXPECT_EQ(result, AWS_OP_SUCCESS);
}
