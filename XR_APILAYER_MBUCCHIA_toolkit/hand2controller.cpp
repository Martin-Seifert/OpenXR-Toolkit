// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "pch.h"

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"
#include <array>

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::input;
    using namespace toolkit::log;

    using namespace xr::math;

    constexpr XrVector3f Bright1{255 / 255.f, 219 / 255.f, 172 / 255.f};
    constexpr XrVector3f Medium1{224 / 255.f, 172 / 255.f, 105 / 255.f};
    constexpr XrVector3f Dark1{141 / 255.f, 85 / 255.f, 36 / 255.f};
    constexpr XrVector3f Darker1{77 / 255.f, 42 / 255.f, 34 / 255.f};

    // Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
    constexpr XrVector3f LBB{-0.5f, -0.5f, -0.5f};
    constexpr XrVector3f LBF{-0.5f, -0.5f, 0.5f};
    constexpr XrVector3f LTB{-0.5f, 0.5f, -0.5f};
    constexpr XrVector3f LTF{-0.5f, 0.5f, 0.5f};
    constexpr XrVector3f RBB{0.5f, -0.5f, -0.5f};
    constexpr XrVector3f RBF{0.5f, -0.5f, 0.5f};
    constexpr XrVector3f RTB{0.5f, 0.5f, -0.5f};
    constexpr XrVector3f RTF{0.5f, 0.5f, 0.5f};

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR)                                                                       \
    {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V5, COLOR}, {V6, COLOR},

#define CUBE_VERTICES(Color)                                                                                           \
    constexpr SimpleMeshVertex c_cube##Color##Vertices[] = {                                                           \
        CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, Color##1) CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Color##1)            \
            CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, Color##1) CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Color##1)        \
                CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, Color##1) CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Color##1)};

    CUBE_VERTICES(Bright);
    CUBE_VERTICES(Medium);
    CUBE_VERTICES(Dark);
    CUBE_VERTICES(Darker);

#undef CUBE_VERTICES
#undef CUBE_SIDE

    constexpr unsigned short c_cubeIndices[] = {
        0,  1,  2,  3,  4,  5,  // -X
        6,  7,  8,  9,  10, 11, // +X
        12, 13, 14, 15, 16, 17, // -Y
        18, 19, 20, 21, 22, 23, // +Y
        24, 25, 26, 27, 28, 29, // -Z
        30, 31, 32, 33, 34, 35, // +Z
    };

    template <typename T, size_t N>
    void copyFromArray(std::vector<T>& targetVector, const T (&sourceArray)[N]) {
        targetVector.assign(sourceArray, sourceArray + N);
    }

    static constexpr uint32_t HandCount = 2;

    static constexpr XrTime GracePeriod = 2000000; // 2ms

    enum class PoseType { Grip, Aim };

    enum class Gesture { Pinch = 0, ThumbPress, IndexBend, FingerGun, Squeeze, Custom1, MaxValue };

    struct ActionSpace {
        Hand hand;
        PoseType poseType;
        XrPosef poseInActionSpace;
    };

    struct SubAction {
        Hand hand;
        std::string path;

        bool synced{false};

        float floatValue{0.0f};
        XrTime timeFloatValueChanged{0};
        bool floatValueChanged{false};

        bool boolValue{false};
        XrTime timeBoolValueChanged{0};
        bool boolValueChanged{false};
    };

    struct Action {
        XrActionSet actionSet;

        std::map<XrPath, SubAction> subActions;
    };

    struct Config {
        Config();

        void ParseConfigurationStatement(const std::string& line, unsigned int lineNumber = 1);
        bool LoadConfiguration(const std::string& configName);
        void Dump();

        std::string interactionProfile;
        bool leftHandEnabled;
        bool rightHandEnabled;

        // The index of the joint (see enum XrHandJointEXT) to use for the aim pose.
        int aimJointIndex;

        // The index of the joint (see enum XrHandJointEXT) to use for the grip pose.
        int gripJointIndex;

        // The threshold (between 0 and 1) when converting a float action into a boolean action and the action is
        // true.
        float clickThreshold;

        // The transformation to apply to the aim and grip poses.
        XrPosef transform[HandCount];

        // The frequency to respond to haptics (NAN for any frequency).
        float hapticsResponseFrequency;

        // The gesture that triggers an action upon haptics.
        Gesture hapticsResponseGesture;

        // The target XrAction path to simulate upon haptics.
        std::string hapticsAction;

        // The interval for emitting keepalive actions.
        XrTime keepaliveInterval;

        // The target XrAction path to simulate keepalive.
        std::string keepaliveAction;

        // The index of the 1st joint (see enum XrHandJointEXT) to use for 1st custom gesture.
        int custom1Joint1Index;

        // The index of the 2nd joint (see enum XrHandJointEXT) to use for 1st custom gesture.
        int custom1Joint2Index;

        // The target XrAction path for a given gesture, and the near/far threshold to map the float action too
        // (near maps to 1, far maps to 0).
#define DEFINE_ACTION(configName)                                                                                      \
    std::string configName##Action[HandCount];                                                                         \
    float configName##Near;                                                                                            \
    float configName##Far;

        DEFINE_ACTION(pinch);
        DEFINE_ACTION(thumbPress);
        DEFINE_ACTION(indexBend);
        DEFINE_ACTION(fingerGun);
        DEFINE_ACTION(squeeze);
        DEFINE_ACTION(palmTap);
        DEFINE_ACTION(wristTap);
        DEFINE_ACTION(indexTipTap);
        DEFINE_ACTION(custom1);

#undef DEFINE_ACTION
    };

    class HandTracker : public IHandTracker {
      public:
        HandTracker(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager)
            : m_openXR(openXR), m_configManager(configManager) {
            // Open a socket for live configuration.
            {
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2, 2), &wsaData);
                m_configSocket = socket(AF_INET, SOCK_DGRAM, 0);
                if (m_configSocket != INVALID_SOCKET) {
                    int one = 1;
                    setsockopt(m_configSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
                    u_long mode = 1; // 1 to enable non-blocking socket
                    ioctlsocket(m_configSocket, FIONBIO, &mode);
                }

                struct sockaddr_in saddr;
                saddr.sin_family = AF_INET;
                saddr.sin_addr.s_addr = INADDR_ANY;
                saddr.sin_port = htons(10001);
                if (m_configSocket == INVALID_SOCKET ||
                    bind(m_configSocket, (const struct sockaddr*)&saddr, sizeof(saddr))) {
                    Log("Failed to create or bind configuration socket\n");
                }
            }

            // Load file configuration.
            m_config.LoadConfiguration(openXR.GetApplicationName());
            m_config.Dump();

            CHECK_HRCMD(openXR.xrStringToPath(
                openXR.GetXrInstance(), m_config.interactionProfile.c_str(), &m_interactionProfile));

            CHECK_HRCMD(m_openXR.xrStringToPath(m_openXR.GetXrInstance(), "/user/hand/left", &m_leftHandSubaction));
            CHECK_HRCMD(m_openXR.xrStringToPath(m_openXR.GetXrInstance(), "/user/hand/right", &m_rightHandSubaction));

            // The remaining resources are created in beginSession().
        }

        ~HandTracker() override {
            endSession();
            closesocket(m_configSocket);
        }

        XrPath getInteractionProfile() const {
            return m_interactionProfile;
        }

        void registerAction(XrAction action, XrActionSet actionSet) override {
            auto actionSetIt = m_actionSets.find(actionSet);
            if (actionSetIt == m_actionSets.end()) {
                actionSetIt = m_actionSets.insert_or_assign(actionSet, std::set<XrAction>()).first;
            }
            actionSetIt->second.insert(action);
        }

        void unregisterAction(XrAction action) override {
            for (auto& actionSet : m_actionSets) {
                actionSet.second.erase(action);
            }
        }

        void registerActionSpace(XrSpace space, const std::string& path, const XrPosef& poseInActionSpace) override {
            ActionSpace actionSpace;

            if (path.find("/user/hand/left") == 0) {
                actionSpace.hand = Hand::Left;
            } else if (path.find("/user/hand/right") == 0) {
                actionSpace.hand = Hand::Right;
            } else {
                assert(false);
            }

            if (path.find("/input/aim/pose") != std::string::npos) {
                actionSpace.poseType = PoseType::Aim;
            } else if (path.find("/input/grip/pose") != std::string::npos) {
                actionSpace.poseType = PoseType::Grip;
            } else {
                assert(false);
            }

            actionSpace.poseInActionSpace = poseInActionSpace;

            DebugLog("Simulating action space %s\n", path.c_str());
            m_actionSpaces.insert_or_assign(space, actionSpace);
        }

        void unregisterActionSpace(XrSpace space) override {
            m_actionSpaces.erase(space);
        }

        void registerBindings(const XrInteractionProfileSuggestedBinding& bindings) override {
            if (bindings.interactionProfile == m_interactionProfile) {
                Log("Binding to interaction profile: %s\n", getPath(m_interactionProfile).c_str());

                // Clear any previous mappings.
                m_actions.clear();

                bool hasSystemClick = false;
                for (uint32_t i = 0; i < bindings.countSuggestedBindings; i++) {
                    const std::string fullPath = getPath(bindings.suggestedBindings[i].binding);

                    XrPath subActionPath;
                    Hand hand;
                    if (fullPath.find("/user/hand/left") == 0) {
                        hand = Hand::Left;
                        subActionPath = m_leftHandSubaction;
                    } else if (fullPath.find("/user/hand/right") == 0) {
                        hand = Hand::Right;
                        subActionPath = m_rightHandSubaction;
                    } else {
                        // We ignore non-hand actions.
                        continue;
                    }

                    static std::string_view systemClickPath = "/input/system/click";
                    // Keep track of the /input/system/click
                    // path.endswith("/input/system/click")
                    if (fullPath.rfind(systemClickPath) == fullPath.length() - systemClickPath.length()) {
                        hasSystemClick = true;
                    }

                    const XrAction action = bindings.suggestedBindings[i].action;
                    auto actionIt = m_actions.find(action);
                    if (actionIt == m_actions.end()) {
                        Action entry;

                        for (auto& actionSet : m_actionSets) {
                            if (actionSet.second.find(action) != actionSet.second.cend()) {
                                entry.actionSet = actionSet.first;
                                break;
                            }
                        }

                        actionIt = m_actions.insert_or_assign(bindings.suggestedBindings[i].action, entry).first;
                    }
                    Action& entry = actionIt->second;

                    SubAction subAction;
                    subAction.hand = hand;
                    subAction.path = fullPath;
                    DebugLog("Simulating action path %s\n", fullPath.c_str());
                    entry.subActions.insert_or_assign(subActionPath, subAction);
                }

                // Dummy action to keep track of /input/system/click in case the application does not register it (which
                // is likely in fact).
                if (!hasSystemClick) {
                    Action systemClick;

                    systemClick.actionSet = XR_NULL_HANDLE;
                    {
                        SubAction subAction;
                        subAction.hand = Hand::Left;
                        subAction.path = "/user/hand/left/input/system/click";
                        systemClick.subActions.insert_or_assign(m_leftHandSubaction, subAction);
                    }
                    {
                        SubAction subAction;
                        subAction.hand = Hand::Left;
                        subAction.path = "/user/hand/right/input/system/click";
                        systemClick.subActions.insert_or_assign(m_rightHandSubaction, subAction);
                    }

                    m_actions.insert_or_assign(XR_NULL_HANDLE, systemClick);
                }
            }
        }

        const std::string getFullPath(XrAction action, XrPath subActionPath) override {
            const auto& actionIt = m_actions.find(action);
            if (actionIt == m_actions.cend()) {
                return {};
            }

            auto subActionIt = actionIt->second.subActions.find(subActionPath);
            if (subActionIt == actionIt->second.subActions.cend()) {
                if (actionIt->second.subActions.empty()) {
                    return {};
                }
                subActionIt = actionIt->second.subActions.begin();
            }

            return subActionIt->second.path;
        }

        void beginSession(XrSession session, std::shared_ptr<toolkit::graphics::IDevice> graphicsDevice) override {
            m_graphicsDevice = graphicsDevice;

            XrHandTrackerCreateInfoEXT leftTrackerCreateInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT, nullptr};
            leftTrackerCreateInfo.hand = XR_HAND_LEFT_EXT;
            leftTrackerCreateInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            XrHandTrackerCreateInfoEXT rightTrackerCreateInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT, nullptr};
            rightTrackerCreateInfo.hand = XR_HAND_RIGHT_EXT;
            rightTrackerCreateInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;

            CHECK_XRCMD(m_openXR.xrCreateHandTrackerEXT(session, &leftTrackerCreateInfo, &m_handTracker[0]));
            CHECK_XRCMD(m_openXR.xrCreateHandTrackerEXT(session, &rightTrackerCreateInfo, &m_handTracker[1]));

            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
            referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
            CHECK_XRCMD(m_openXR.xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_referenceSpace));

            std::vector<uint16_t> indices;
            copyFromArray(indices, c_cubeIndices);
            std::vector<SimpleMeshVertex> vertices;

            copyFromArray(vertices, c_cubeBrightVertices);
            m_jointMesh.push_back(m_graphicsDevice->createSimpleMesh(vertices, indices, "Joint Mesh"));
            copyFromArray(vertices, c_cubeMediumVertices);
            m_jointMesh.push_back(m_graphicsDevice->createSimpleMesh(vertices, indices, "Joint Mesh"));
            copyFromArray(vertices, c_cubeDarkVertices);
            m_jointMesh.push_back(m_graphicsDevice->createSimpleMesh(vertices, indices, "Joint Mesh"));
            copyFromArray(vertices, c_cubeDarkerVertices);
            m_jointMesh.push_back(m_graphicsDevice->createSimpleMesh(vertices, indices, "Joint Mesh"));
        }

        void endSession() override {
            m_graphicsDevice.reset();
            m_jointMesh.clear();

            if (m_referenceSpace != XR_NULL_HANDLE) {
                m_openXR.xrDestroySpace(m_referenceSpace);
                m_referenceSpace = XR_NULL_HANDLE;
            }
            for (uint32_t i = 0; i < HandCount; i++) {
                if (m_handTracker[i] != XR_NULL_HANDLE) {
                    m_openXR.xrDestroyHandTrackerEXT(m_handTracker[i]);
                    m_handTracker[i] = XR_NULL_HANDLE;
                }
            }
        }

        void sync(XrTime frameTime, XrTime now, const XrActionsSyncInfo& syncInfo) override {
            // Arbitrarily choose this place to handle configuration input.
            if (m_configSocket != INVALID_SOCKET) {
                struct sockaddr_in saddr;
                while (true) {
                    char buffer[100] = {};
                    int slen = sizeof(saddr);
                    const int len =
                        recvfrom(m_configSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&saddr, &slen);
                    if (len <= 0) {
                        break;
                    }

                    std::string line(buffer);
                    m_config.ParseConfigurationStatement(line);
                }
            }

            // Delete outdated entries from the cache.
            {
                std::unique_lock lock(m_cacheLock);
                for (auto& spaceCache : m_cachedHandJointsPoses) {
                    auto& cache = spaceCache.second;
                    for (uint32_t side = 0; side < HandCount; side++) {
                        while (cache[side].size() > 0 && cache[side].front().first + GracePeriod < now) {
                            cache[side].pop_front();
                        }

                        // Update statistics.
                        if (spaceCache.first == m_preferredBaseSpace.value_or(m_referenceSpace)) {
                            m_gesturesState.cacheSize[side] = cache[side].size();
                        }
                    }
                }
            }

            // Inhibit one and or the other if request. The config file acts as a global override.
            const auto handTrackingEnabled =
                m_configManager->getEnumValue<HandTrackingEnabled>(SettingHandTrackingEnabled);
            m_leftHandEnabled = m_config.leftHandEnabled && (handTrackingEnabled == HandTrackingEnabled::Both ||
                                                             handTrackingEnabled == HandTrackingEnabled::Left);
            m_rightHandEnabled = m_config.rightHandEnabled && (handTrackingEnabled == HandTrackingEnabled::Both ||
                                                               handTrackingEnabled == HandTrackingEnabled::Right);

            // Get joints poses.
            const XrHandJointLocationEXT* leftHandJointsPoses = nullptr;
            if (m_leftHandEnabled) {
                leftHandJointsPoses = getCachedHandJointsPoses(
                    Hand::Left, m_thisFrameTime, now, m_preferredBaseSpace.value_or(m_referenceSpace));
            }
            const XrHandJointLocationEXT* rightHandJointsPoses = nullptr;
            if (m_rightHandEnabled) {
                rightHandJointsPoses = getCachedHandJointsPoses(
                    Hand::Right, m_thisFrameTime, now, m_preferredBaseSpace.value_or(m_referenceSpace));
            }

            // Only sync actions for the specified action sets.
            std::set<XrAction> ignore;
            const Action* systemClick = nullptr;
            for (auto& action : m_actions) {
                bool foundActionSet = false;
                for (uint32_t i = 0; i < syncInfo.countActiveActionSets; i++) {
                    if (action.second.actionSet == XR_NULL_HANDLE ||
                        action.second.actionSet == syncInfo.activeActionSets[i].actionSet) {
                        // TODO: We ignore the subActionPath at this time. This is largely OK and mean we might be
                        // non-compliant to some edge cases.
                        foundActionSet = true;
                        break;
                    }
                }

                if (!foundActionSet) {
                    ignore.insert(action.first);
                }

                const auto& subAction = *action.second.subActions.cbegin();
                const auto& path = subAction.second.path;
                static std::string_view systemClickPath = "/input/system/click";
                // Keep track of the /input/system/click
                // path.endswith("/input/system/click")
                if (path.rfind(systemClickPath) == path.length() - systemClickPath.length()) {
                    systemClick = &action.second;
                }

                for (auto& subAction : action.second.subActions) {
                    subAction.second.synced = false;
                }
            }

            // For each gesture, update the action value.
            performGesturesDetection(leftHandJointsPoses, rightHandJointsPoses, ignore, now);

            // Check for keepalive.
            if (!m_config.keepaliveAction.empty() && m_config.keepaliveInterval) {
                if (now - m_lastKeepalive > m_config.keepaliveInterval) {
                    if (m_lastKeepalive) {
                        for (uint32_t side = 0; side < HandCount; side++) {
                            const Hand hand = (Hand)side;
                            recordActionValue(hand, m_config.keepaliveAction, ignore, 1.f, now);
                        }
                    }

                    m_lastKeepalive = now;
                }
            }

            // Special handling for Windows key.
            if (systemClick) {
                bool didChange = false;
                bool value = false;

                for (auto& subAction : systemClick->subActions) {
                    didChange = didChange || subAction.second.boolValueChanged;
                    value = value || subAction.second.boolValue;
                }

                if (didChange && value) {
                    INPUT input[2];
                    ZeroMemory(input, sizeof(input));
                    input[0].type = INPUT_KEYBOARD;
                    input[0].ki.wVk = VK_LWIN;
                    input[1].type = INPUT_KEYBOARD;
                    input[1].ki.wVk = VK_LWIN;
                    input[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(2, input, sizeof(INPUT));
                }
            }

            const int64_t timeout = m_configManager->getValue(SettingHandTimeout);
            for (uint32_t side = 0; side < HandCount; side++) {
                if (timeout == 0) {
                    m_trackedRecently[side] = true;
                    continue;
                }

                bool tracked = m_lastTimestampWithPoseTracked[side] + (timeout * 1000000000) > m_thisFrameTime;

                // On transition, zero all actions. We need to do this in two steps, otherwise the application will not
                // see the zero'ed action. If any action was not 0, we set the tracked bit again to give the app one
                // more chance to see the changes.
                if (m_trackedRecently[side] && !tracked) {
                    const XrPath subactionPath = side == 0 ? m_leftHandSubaction : m_rightHandSubaction;
                    for (auto& action : m_actions) {
                        auto subActionIt = action.second.subActions.find(subactionPath);
                        if (subActionIt == action.second.subActions.cend()) {
                            if (action.second.subActions.empty()) {
                                continue;
                            }
                            const auto& fullPath = action.second.subActions.begin()->second.path;
                            if ((side == 0 && fullPath.find("/user/hand/left") != 0) ||
                                (side == 1 && fullPath.find("/user/hand/right") != 0)) {
                                continue;
                            }
                        }
                        auto& subAction = subActionIt->second;
                        // We must only set changed if the value is actually different.
                        subAction.floatValueChanged = std::abs(subAction.floatValue) > FLT_EPSILON;
                        subAction.floatValue = 0.f;
                        if (subAction.floatValueChanged) {
                            subAction.timeFloatValueChanged = now;
                            tracked = true;
                        }

                        subAction.boolValueChanged = subAction.boolValue;
                        subAction.boolValue = false;
                        if (subAction.boolValueChanged) {
                            subAction.timeBoolValueChanged = now;
                            tracked = true;
                        }
                    }

                    // Clear statistics.
                    m_gesturesState.handposeAgeUs[side] = 0;
                }
                m_trackedRecently[side] = tracked;
            }

            m_thisFrameTime = frameTime;
        }

        bool
        locate(XrSpace space, XrSpace baseSpace, XrTime time, XrTime now, XrSpaceLocation& location) const override {
            const auto actionSpaceIt = m_actionSpaces.find(space);
            if (actionSpaceIt == m_actionSpaces.cend()) {
                return false;
            }

            const auto& actionSpace = actionSpaceIt->second;

            if ((actionSpace.hand == Hand::Left && !m_leftHandEnabled) ||
                (actionSpace.hand == Hand::Right && !m_rightHandEnabled)) {
                return false;
            }

            const auto& jointsPoses = getCachedHandJointsPoses(actionSpace.hand, time, now, baseSpace);

            const uint32_t side = actionSpace.hand == Hand::Left ? 0 : 1;
            const uint32_t joint =
                actionSpace.poseType == PoseType::Grip ? m_config.gripJointIndex : m_config.aimJointIndex;

            // Translate the hand poses for the requested joint to a controller pose.
            location.locationFlags = jointsPoses[joint].locationFlags;
            if (Pose::IsPoseValid(location.locationFlags)) {
                // Workaround to loss of virtual controller: if the pose is Valid, we always report it Tracked.
                location.locationFlags |=
                    XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
            }
            location.pose = Pose::Multiply(actionSpace.poseInActionSpace,
                                           Pose::Multiply(m_config.transform[side], jointsPoses[joint].pose));

            m_gesturesState.handposeAgeUs[side] = std::max(m_gesturesState.handposeAgeUs[side], time - now);

            return true;
        }

        void render(const XrPosef& pose,
                    XrSpace baseSpace,
                    XrTime now,
                    std::shared_ptr<graphics::ITexture> renderTarget) const override {
            const int meshIndex = m_configManager->getValue(SettingHandVisibilityAndSkinTone) - 1;
            if (meshIndex < 0) {
                return;
            }

            for (uint32_t hand = 0; hand < HandCount; hand++) {
                if ((!m_leftHandEnabled && hand == 0) || (!m_rightHandEnabled && hand == 1)) {
                    continue;
                }

                const auto& jointsPoses =
                    getCachedHandJointsPoses(hand ? Hand::Right : Hand::Left, m_thisFrameTime, now, baseSpace);

                for (uint32_t joint = 0; joint < XR_HAND_JOINT_COUNT_EXT; joint++) {
                    if (!xr::math::Pose::IsPoseValid(jointsPoses[joint].locationFlags)) {
                        continue;
                    }

                    XrVector3f scaling{jointsPoses[joint].radius,
                                       std::min(0.0025f, jointsPoses[joint].radius),
                                       std::max(0.015f, jointsPoses[joint].radius)};
                    m_graphicsDevice->draw(m_jointMesh[meshIndex], jointsPoses[joint].pose, scaling);
                }
            }

            // The sync() method only cares for relative hand joints poses. Try to force reuse of cached entries by
            // making sync() query with the same base space.
            m_preferredBaseSpace = baseSpace;
        }

        bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateBoolean& state) const override {
            const auto& actionIt = m_actions.find(getInfo.action);
            if (actionIt == m_actions.cend()) {
                return false;
            }
            const auto& action = actionIt->second;

            auto subActionIt = action.subActions.find(getInfo.subactionPath);
            if (subActionIt == actionIt->second.subActions.cend()) {
                if (actionIt->second.subActions.empty()) {
                    return false;
                }
                subActionIt = actionIt->second.subActions.begin();
            }
            const auto& subAction = subActionIt->second;

            if ((subAction.hand == Hand::Left && !m_leftHandEnabled) ||
                (subAction.hand == Hand::Right && !m_rightHandEnabled)) {
                return false;
            }

            state.isActive = XR_TRUE;
            state.currentState = subAction.boolValue;
            state.changedSinceLastSync = subAction.boolValueChanged;
            state.lastChangeTime = subAction.timeBoolValueChanged;

            return true;
        }

        bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateFloat& state) const override {
            const auto& actionIt = m_actions.find(getInfo.action);
            if (actionIt == m_actions.cend()) {
                return false;
            }
            const auto& action = actionIt->second;

            auto subActionIt = action.subActions.find(getInfo.subactionPath);
            if (subActionIt == actionIt->second.subActions.cend()) {
                if (actionIt->second.subActions.empty()) {
                    return false;
                }
                subActionIt = actionIt->second.subActions.begin();
            }
            const auto& subAction = subActionIt->second;

            if ((subAction.hand == Hand::Left && !m_leftHandEnabled) ||
                (subAction.hand == Hand::Right && !m_rightHandEnabled)) {
                return false;
            }

            state.isActive = XR_TRUE;
            state.currentState = subAction.floatValue;
            state.changedSinceLastSync = subAction.floatValueChanged;
            state.lastChangeTime = subAction.timeFloatValueChanged;

            return true;
        }

        bool isHandEnabled(Hand hand) const override {
            if ((hand == Hand::Left && !m_leftHandEnabled) || (hand == Hand::Right && !m_rightHandEnabled)) {
                return false;
            }

            return true;
        }

        bool isTrackedRecently(Hand hand) const override {
            const uint32_t side = hand == Hand::Left ? 0 : 1;
            return m_trackedRecently[side];
        }

        void handleOutput(Hand hand, float frequency, XrDuration duration) override {
            if ((hand == Hand::Left && !m_leftHandEnabled) || (hand == Hand::Right && !m_rightHandEnabled)) {
                return;
            }

            const uint32_t side = hand == Hand::Left ? 0 : 1;
            if (isnan(frequency)) {
                m_gesturesState.hapticsFrequency[side] = NAN;
                m_gesturesState.hapticsDurationUs[side] = -2;
                return;
            }

            m_gesturesState.hapticsFrequency[side] = frequency;
            m_gesturesState.hapticsDurationUs[side] = duration;

            // Filter on frequency value.
            if ((isnan(m_config.hapticsResponseFrequency) ||
                 std::abs(m_config.hapticsResponseFrequency - frequency) < FLT_EPSILON)) {
                m_evaluateHapticsGesture = true;
            }
        }

        const GesturesState& getGesturesState() const override {
            return m_gesturesState;
        }

      private:
        const std::string getPath(XrPath path) {
            char buf[XR_MAX_PATH_LENGTH];
            uint32_t count;
            CHECK_XRCMD(m_openXR.xrPathToString(m_openXR.GetXrInstance(), path, sizeof(buf), &count, buf));
            std::string str;
            str.assign(buf, count - 1);
            return str;
        }

        const XrHandJointLocationEXT*
        getCachedHandJointsPoses(Hand hand, XrTime time, XrTime now, std::optional<XrSpace> baseSpace) const {
            const uint32_t side = hand == Hand::Left ? 0 : 1;

            std::unique_lock lock(m_cacheLock);

            std::deque<CacheEntry> cache = m_cachedHandJointsPoses[baseSpace.value_or(m_referenceSpace)][side];

            // Search for a entry in the cache.
            int32_t closestIndex = -1;
            std::deque<CacheEntry>::iterator insertIt = cache.begin();
            XrTime closestTimeDelta = INT64_MAX;
            for (uint32_t i = 0; i < cache.size(); i++) {
                const auto t = cache[i].first;
                const XrTime delta = std::abs(t - time);
                if (t < time) {
                    insertIt++;
                }

                if (delta < closestTimeDelta) {
                    closestTimeDelta = delta;
                    closestIndex = i;
                } else {
                    break;
                }
            }

            if (closestIndex != -1 && closestTimeDelta < GracePeriod) {
                return cache[closestIndex].second.data();
            }

            // Create a new entry.
            {
                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = baseSpace.value_or(m_referenceSpace);
                // Workaround to loss of virtual controller: do not query a time in the past!
                locateInfo.time = std::max(time, now);

                CacheEntry entry;
                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT, nullptr};
                locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
                locations.jointLocations = (entry.second).data();
                entry.first = time;

                CHECK_HRCMD(m_openXR.xrLocateHandJointsEXT(m_handTracker[side], &locateInfo, &locations));
                if (Pose::IsPoseTracked(locations.jointLocations[XR_HAND_JOINT_PALM_EXT].locationFlags)) {
                    m_lastTimestampWithPoseTracked[side] = std::max(time, m_lastTimestampWithPoseTracked[side]);
                } else {
                    m_gesturesState.numTrackingLosses[side]++;
                }
                
                std::deque<CacheEntry>::iterator ret = cache.emplace(insertIt, entry);
                return ret->second.data();
            }
        }

        void performGesturesDetection(const XrHandJointLocationEXT* leftHandJointsPoses,
                                      const XrHandJointLocationEXT* rightHandJointsPoses,
                                      const std::set<XrAction>& ignore,
                                      XrTime now) {
#define ACTION_PARAMS(configName) m_config.configName##Action[side],

            for (uint32_t side = 0; side < HandCount; side++) {
                const Hand hand = (Hand)side;
                const XrHandJointLocationEXT* jointsPoses =
                    hand == Hand::Left ? leftHandJointsPoses : rightHandJointsPoses;
                const XrHandJointLocationEXT* jointsPosesOtherHand =
                    hand == Hand::Left ? rightHandJointsPoses : leftHandJointsPoses;

                if (!jointsPoses) {
                    continue;
                }

                const Gesture hapticsGesture =
                    !m_config.hapticsAction.empty() ? m_config.hapticsResponseGesture : Gesture::MaxValue;
                bool hapticsGestureState = false;

#define ONE_HANDED_GESTURE(configName, gesture, joint1, joint2)                                                        \
    do {                                                                                                               \
        if (!m_config.configName##Action[side].empty() || hapticsGesture == gesture) {                                 \
            const auto value = computeJointActionValue(                                                                \
                jointsPoses, (joint1), jointsPoses, (joint2), m_config.configName##Near, m_config.configName##Far);    \
            m_gesturesState.configName##Value[side] = value;                                                           \
            if (!m_config.configName##Action[side].empty()) {                                                          \
                recordActionValue(hand, m_config.configName##Action[side], ignore, value, now);                        \
            }                                                                                                          \
            if (hapticsGesture == gesture) {                                                                           \
                hapticsGestureState = value >= m_config.clickThreshold;                                                \
            }                                                                                                          \
        }                                                                                                              \
    } while (false);

                // Handle gestures made up from one hand.
                ONE_HANDED_GESTURE(pinch, Gesture::Squeeze, XR_HAND_JOINT_THUMB_TIP_EXT, XR_HAND_JOINT_INDEX_TIP_EXT);
                ONE_HANDED_GESTURE(
                    thumbPress, Gesture::ThumbPress, XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT, XR_HAND_JOINT_THUMB_TIP_EXT);
                ONE_HANDED_GESTURE(
                    indexBend, Gesture::IndexBend, XR_HAND_JOINT_INDEX_PROXIMAL_EXT, XR_HAND_JOINT_INDEX_TIP_EXT);
                ONE_HANDED_GESTURE(
                    fingerGun, Gesture::FingerGun, XR_HAND_JOINT_THUMB_TIP_EXT, XR_HAND_JOINT_MIDDLE_INTERMEDIATE_EXT);

                if (m_config.custom1Joint1Index >= 0 && m_config.custom1Joint2Index >= 0) {
                    ONE_HANDED_GESTURE(
                        custom1, Gesture::Custom1, m_config.custom1Joint1Index, m_config.custom1Joint2Index);
                }

#undef ONE_HANDED_GESTURE

                // Squeeze requires to look at 3 fingers.
                if (!m_config.squeezeAction[side].empty() || hapticsGesture == Gesture::Squeeze) {
                    float squeeze[3] = {computeJointActionValue(jointsPoses,
                                                                XR_HAND_JOINT_MIDDLE_TIP_EXT,
                                                                jointsPoses,
                                                                XR_HAND_JOINT_MIDDLE_METACARPAL_EXT,
                                                                m_config.squeezeNear,
                                                                m_config.squeezeFar),
                                        computeJointActionValue(jointsPoses,
                                                                XR_HAND_JOINT_RING_TIP_EXT,
                                                                jointsPoses,
                                                                XR_HAND_JOINT_RING_METACARPAL_EXT,
                                                                m_config.squeezeNear,
                                                                m_config.squeezeFar),
                                        computeJointActionValue(jointsPoses,
                                                                XR_HAND_JOINT_LITTLE_TIP_EXT,
                                                                jointsPoses,
                                                                XR_HAND_JOINT_LITTLE_METACARPAL_EXT,
                                                                m_config.squeezeNear,
                                                                m_config.squeezeFar)};

                    // Quickly bubble sort.
                    if (squeeze[0] > squeeze[1]) {
                        std::swap(squeeze[0], squeeze[1]);
                    }
                    if (squeeze[0] > squeeze[2]) {
                        std::swap(squeeze[0], squeeze[2]);
                    }
                    if (squeeze[1] > squeeze[2]) {
                        std::swap(squeeze[1], squeeze[2]);
                    }

                    // Ignore the lowest value, average the other ones.
                    const float value = (squeeze[1] + squeeze[2]) / 2.f;
                    m_gesturesState.squeezeValue[side] = value;
                    if (!m_config.squeezeAction[side].empty()) {
                        recordActionValue(hand, m_config.squeezeAction[side], ignore, value, now);
                    }
                    if (hapticsGesture == Gesture::Squeeze) {
                        hapticsGestureState = value >= m_config.clickThreshold;
                    }
                }

                // Check for haptics trigger.
                if (m_evaluateHapticsGesture && !m_config.hapticsAction.empty() && hapticsGestureState) {
                    recordActionValue(hand, m_config.hapticsAction, ignore, 1.f, now);
                }

#define TWO_HANDED_GESTURE(configName, joint1, joint2)                                                                 \
    do {                                                                                                               \
        if (!m_config.configName##Action[side].empty()) {                                                              \
            const auto value = computeJointActionValue(jointsPoses,                                                    \
                                                       (joint1),                                                       \
                                                       jointsPosesOtherHand,                                           \
                                                       (joint2),                                                       \
                                                       m_config.configName##Near,                                      \
                                                       m_config.configName##Far);                                      \
            m_gesturesState.configName##Value[side] = value;                                                           \
            recordActionValue(hand, m_config.configName##Action[side], ignore, value, now);                            \
        }                                                                                                              \
    } while (false);

                if (!jointsPosesOtherHand) {
                    continue;
                }

                // Handle gestures made up using both hands.

                TWO_HANDED_GESTURE(palmTap, XR_HAND_JOINT_PALM_EXT, XR_HAND_JOINT_INDEX_TIP_EXT);
                TWO_HANDED_GESTURE(wristTap, XR_HAND_JOINT_WRIST_EXT, XR_HAND_JOINT_INDEX_TIP_EXT);
                TWO_HANDED_GESTURE(indexTipTap, XR_HAND_JOINT_INDEX_TIP_EXT, XR_HAND_JOINT_INDEX_TIP_EXT);

#undef TWO_HANDED_GESTURE
            }

            m_evaluateHapticsGesture = false;
        }

        // Compute the scaled action value based on the distance between 2 joints.
        float computeJointActionValue(const XrHandJointLocationEXT* joints1Poses,
                                      uint32_t joint1,
                                      const XrHandJointLocationEXT* joints2Poses,
                                      uint32_t joint2,
                                      float nearDistance,
                                      float farDistance) {
            if (Pose::IsPoseValid(joints1Poses[joint1].locationFlags) &&
                Pose::IsPoseValid(joints2Poses[joint2].locationFlags)) {
                // We ignore joints radius and assume the near/far distance are configured to account for them.
                const float distance =
                    std::max(Length(joints1Poses[joint1].pose.position - joints2Poses[joint2].pose.position), 0.f);

                return 1.f -
                       (std::clamp(distance, nearDistance, farDistance) - nearDistance) / (farDistance - nearDistance);
            }
            return NAN;
        }

        void recordActionValue(
            Hand hand, const std::string& actionPath, const std::set<XrAction>& ignore, float value, XrTime now) {
            assert(!actionPath.empty());

            if (isnan(value)) {
                return;
            }

            const XrPath subActionPath = hand == Hand::Left ? m_leftHandSubaction : m_rightHandSubaction;

            for (auto& action : m_actions) {
                if (ignore.find(action.first) != ignore.cend()) {
                    continue;
                }

                auto& subAction = action.second.subActions[subActionPath];
                const std::string& path = subAction.path;

                // path.endswith(actionPath)
                if (path.rfind(actionPath) == path.length() - actionPath.length()) {
                    // If multiple gestures are bound to the same action, pick the highest value.
                    const float newFloatValue = subAction.synced ? std::max(subAction.floatValue, value) : value;
                    const bool newBoolValue = newFloatValue >= m_config.clickThreshold;

                    if (std::abs(subAction.floatValue - newFloatValue) > FLT_EPSILON) {
                        subAction.floatValue = newFloatValue;
                        subAction.timeFloatValueChanged = now;
                        subAction.floatValueChanged = true;
                    }
                    if (subAction.boolValue != newBoolValue) {
                        subAction.boolValue = newBoolValue;
                        subAction.timeBoolValueChanged = now;
                        subAction.boolValueChanged = true;
                    }
                    subAction.synced = true;
                }
            }
        }

        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;

        XrPath m_leftHandSubaction{XR_NULL_PATH};
        XrPath m_rightHandSubaction{XR_NULL_PATH};

        XrSpace m_referenceSpace{XR_NULL_HANDLE};

        Config m_config;
        SOCKET m_configSocket{INVALID_SOCKET};
        XrPath m_interactionProfile{XR_NULL_PATH};

        std::shared_ptr<IDevice> m_graphicsDevice;
        // One mesh for each color.
        std::vector<std::shared_ptr<ISimpleMesh>> m_jointMesh;

        XrHandTrackerEXT m_handTracker[HandCount]{XR_NULL_HANDLE, XR_NULL_HANDLE};
        XrTime m_thisFrameTime{0};

        bool m_leftHandEnabled{true};
        bool m_rightHandEnabled{true};

        std::map<XrSpace, ActionSpace> m_actionSpaces;
        std::map<XrActionSet, std::set<XrAction>> m_actionSets;
        std::map<XrAction, Action> m_actions;

        bool m_trackedRecently[2]{false, false};
        bool m_evaluateHapticsGesture{false};
        XrTime m_lastKeepalive{0};

        using CacheEntry = std::pair<XrTime, std::array<XrHandJointLocationEXT, XR_HAND_JOINT_COUNT_EXT>>;
        mutable std::map<XrSpace, std::deque<CacheEntry>[HandCount]> m_cachedHandJointsPoses;
        mutable std::mutex m_cacheLock;
        mutable std::optional<XrSpace> m_preferredBaseSpace;
        mutable XrTime m_lastTimestampWithPoseTracked[HandCount]{0, 0};
        mutable GesturesState m_gesturesState{};
    };

    Config::Config() {
        interactionProfile = "/interaction_profiles/hp/mixed_reality_controller";
        leftHandEnabled = true;
        rightHandEnabled = true;
        aimJointIndex = XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT;
        gripJointIndex = XR_HAND_JOINT_PALM_EXT;
        clickThreshold = 0.75f;
        transform[0] = transform[1] = Pose::Identity();
        hapticsResponseFrequency = NAN;
        hapticsResponseGesture = Gesture::FingerGun;
        hapticsAction = "";
        keepaliveInterval = 0;
        keepaliveAction = "";
        pinchAction[0] = pinchAction[1] = "";
        pinchNear = 0.0f;
        pinchFar = 0.05f;
        thumbPressAction[0] = thumbPressAction[1] = "";
        thumbPressNear = 0.0f;
        thumbPressFar = 0.05f;
        indexBendAction[0] = indexBendAction[1] = "/input/trigger/value";
        indexBendNear = 0.045f;
        indexBendFar = 0.07f;
        fingerGunAction[0] = fingerGunAction[1] = "";
        fingerGunNear = 0.01f;
        fingerGunFar = 0.03f;
        squeezeAction[0] = squeezeAction[1] = "/input/squeeze/value";
        squeezeNear = 0.035f;
        squeezeFar = 0.07f;
        palmTapAction[0] = palmTapAction[1] = "";
        palmTapNear = 0.02f;
        palmTapFar = 0.06f;
        wristTapAction[0] = "/input/menu/click";
        wristTapAction[1] = "";
        wristTapNear = 0.04f;
        wristTapFar = 0.05f;
        indexTipTapAction[0] = "";
        indexTipTapAction[1] = "/input/b/click";
        indexTipTapNear = 0.0f;
        indexTipTapFar = 0.07f;
        // Custom gesture is unconfigured.
        custom1Action[0] = custom1Action[1] = "";
        custom1Joint1Index = -1;
        custom1Joint2Index = -1;
        custom1Near = 0.0f;
        custom1Far = 0.1f;
    }

    void Config::ParseConfigurationStatement(const std::string& line, unsigned int lineNumber) {
        try {
            const auto offset = line.find('=');
            if (offset != std::string::npos) {
                const std::string name = line.substr(0, offset);
                const std::string value = line.substr(offset + 1);
                std::string subName;
                int side = -1;

                if (line.substr(0, 5) == "left.") {
                    side = 0;
                    subName = name.substr(5);
                } else if (line.substr(0, 6) == "right.") {
                    side = 1;
                    subName = name.substr(6);
                }

                if (name == "interaction_profile") {
                    interactionProfile = value;
                } else if (name == "aim_joint") {
                    aimJointIndex = std::stoi(value);
                } else if (name == "grip_joint") {
                    gripJointIndex = std::stoi(value);
                } else if (name == "custom1_joint1") {
                    custom1Joint1Index = std::stoi(value);
                } else if (name == "custom1_joint2") {
                    custom1Joint2Index = std::stoi(value);
                } else if (name == "click_threshold") {
                    clickThreshold = std::stof(value);
                } else if (name == "haptics_frequency") {
                    hapticsResponseFrequency = std::stof(value);
                } else if (name == "haptics_gesture") {
                    hapticsResponseGesture = (Gesture)std::stoi(value);
                } else if (name == "haptics_action") {
                    hapticsAction = value;
                } else if (name == "keepalive_interval") {
                    keepaliveInterval = (XrTime)(std::stof(value) * 1e9);
                } else if (name == "keepalive_action") {
                    keepaliveAction = value;
                } else if (side >= 0 && subName == "enabled") {
                    const bool boolValue = value == "1" || value == "true";
                    if (side == 0) {
                        leftHandEnabled = boolValue;
                    } else {
                        rightHandEnabled = boolValue;
                    }
                } else if (side >= 0 && subName == "transform.vec") {
                    std::stringstream ss(value);
                    std::string component;
                    std::getline(ss, component, ' ');
                    transform[side].position.x = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].position.y = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].position.z = std::stof(component);
                } else if (side >= 0 && subName == "transform.quat") {
                    std::stringstream ss(value);
                    std::string component;
                    std::getline(ss, component, ' ');
                    transform[side].orientation.x = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].orientation.y = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].orientation.z = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].orientation.w = std::stof(component);
                } else if (side >= 0 && subName == "transform.euler") {
                    // For UI use only.
                }
#define PARSE_ACTION(configString, configName)                                                                         \
    else if (side >= 0 && subName == configString) {                                                                   \
        configName##Action[side] = value;                                                                              \
    }                                                                                                                  \
    else if (name == configString ".near") {                                                                           \
        configName##Near = std::stof(value);                                                                           \
    }                                                                                                                  \
    else if (name == configString ".far") {                                                                            \
        configName##Far = std::stof(value);                                                                            \
    }

                PARSE_ACTION("pinch", pinch)
                PARSE_ACTION("thumb_press", thumbPress)
                PARSE_ACTION("index_bend", indexBend)
                PARSE_ACTION("finger_gun", fingerGun)
                PARSE_ACTION("squeeze", squeeze)
                PARSE_ACTION("palm_tap", palmTap)
                PARSE_ACTION("wrist_tap", wristTap)
                PARSE_ACTION("index_tip_tap", indexTipTap)
                PARSE_ACTION("custom1", custom1)

#undef PARSE_ACTION
                else {
                    Log("L%u: Unrecognized option\n", lineNumber);
                }
            } else {
                Log("L%u: Improperly formatted option\n", lineNumber);
            }
        } catch (...) {
            Log("L%u: Parsing error\n", lineNumber);
        }
    }

    bool Config::LoadConfiguration(const std::string& configName) {
        std::ifstream configFile;

        // Look in %LocalAppData% first, then fallback to your installation folder.
        configFile.open(localAppData / "configs" / (configName + ".cfg"));
        if (!configFile.is_open()) {
            configFile.open(dllHome / (configName + ".cfg"));
        }

        if (configFile.is_open()) {
            Log("Loading config for \"%s\"\n", configName.c_str());

            unsigned int lineNumber = 0;
            std::string line;
            while (std::getline(configFile, line)) {
                lineNumber++;
                ParseConfigurationStatement(line, lineNumber);
            }
            configFile.close();

            return true;
        }

        Log("Could not load config for \"%s\"\n", configName.c_str());

        return false;
    }

    void Config::Dump() {
        Log("Emulating interaction profile: %s\n", interactionProfile.c_str());
        if (leftHandEnabled) {
            Log("Left transform: (%.3f, %.3f, %.3f) (%.3f, %.3f, %.3f, %.3f)\n",
                transform[0].position.x,
                transform[0].position.y,
                transform[0].position.z,
                transform[0].orientation.x,
                transform[0].orientation.y,
                transform[0].orientation.z,
                transform[0].orientation.w);
        }
        if (rightHandEnabled) {
            Log("Right transform: (%.3f, %.3f, %.3f) (%.3f, %.3f, %.3f, %.3f)\n",
                transform[1].position.x,
                transform[1].position.y,
                transform[1].position.z,
                transform[1].orientation.x,
                transform[1].orientation.y,
                transform[1].orientation.z,
                transform[1].orientation.w);
        }
        if (leftHandEnabled || rightHandEnabled) {
            Log("Grip pose uses joint: %d\n", gripJointIndex);
            Log("Aim pose uses joint: %d\n", aimJointIndex);
            Log("Click threshold: %.3f\n", clickThreshold);
        }
        if (!hapticsAction.empty()) {
            if (!isnan(hapticsResponseFrequency)) {
                Log("Haptics filter on %.3f Hz vibration\n", hapticsResponseFrequency);
            }
            Log("Haptics translates to: %s (on gesture %u)\n", hapticsAction.c_str(), hapticsResponseGesture);
        }
        if (!keepaliveAction.empty() && keepaliveInterval) {
            Log("Keepalive every %llu ns: %s\n", keepaliveInterval, keepaliveAction.c_str());
        }
        if (custom1Joint1Index >= 0 && custom1Joint2Index >= 0) {
            Log("Custom gesture uses joints: %d %d\n", custom1Joint1Index, custom1Joint2Index);
        }
        for (int side = 0; side < HandCount; side++) {
            if ((side == 0 && !leftHandEnabled) || (side == 1 && !rightHandEnabled)) {
                continue;
            }

#define LOG_IF_SET(actionName, configName)                                                                             \
    if (!configName##Action[side].empty()) {                                                                           \
        Log("%s hand " #actionName " translates to: %s (near: %.3f, far: %.3f)\n",                                     \
            side ? "Right" : "Left",                                                                                   \
            configName##Action[side].c_str(),                                                                          \
            configName##Near,                                                                                          \
            configName##Far);                                                                                          \
    }

            LOG_IF_SET("pinch", pinch);
            LOG_IF_SET("thumb press", thumbPress);
            LOG_IF_SET("index bend", indexBend);
            LOG_IF_SET("finger gun", fingerGun);
            LOG_IF_SET("squeeze", squeeze);
            LOG_IF_SET("palm tap", palmTap);
            LOG_IF_SET("wrist tap", wristTap);
            LOG_IF_SET("index tip tap", indexTipTap);
            LOG_IF_SET("custom gesture", custom1);

#undef LOG_IF_SET
        }
    }

} // namespace

namespace toolkit::input {

    std::shared_ptr<IHandTracker> CreateHandTracker(toolkit::OpenXrApi& openXR,
                                                    std::shared_ptr<toolkit::config::IConfigManager> configManager) {
        return std::make_shared<HandTracker>(openXR, configManager);
    }

} // namespace toolkit::input
