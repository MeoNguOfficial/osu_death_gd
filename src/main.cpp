#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chrono> // Thư viện thời gian thực

using namespace geode::prelude;

static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;
// Lưu thời điểm bắt đầu hiệu ứng
static std::chrono::steady_clock::time_point s_startTime;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        // Tính toán Pitch dựa trên thời gian thực trôi qua
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_startTime).count() / 1000.0f;
        
        // Công thức: Pitch giảm dần từ 1.0 về 0.01 dựa trên thời gian thực
        s_targetPitch = 1.0f - (elapsed / s_duration);
        if (s_targetPitch < 0.01f) s_targetPitch = 0.01f;

        // 1. ÉP PITCH MASTER (SFX)
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) masterGroup->setPitch(s_targetPitch);
        }

        // 2. ÉP MUSIC (Dùng Safe Cast cho 2.2081)
        if (m_backgroundMusicChannel) {
            auto channel = reinterpret_cast<FMOD::Channel*>(m_backgroundMusicChannel);
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);

            FMOD::ChannelGroup* musicGroup = nullptr;
            if (channel && channel->getChannelGroup(&musicGroup) == FMOD_OK && musicGroup) {
                musicGroup->setPitch(s_targetPitch);
                musicGroup->setPaused(false);
            }
        }
    }

    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;
        s_startTime = std::chrono::steady_clock::now(); // Ghi lại lúc chết

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speedValue);
        if (s_duration < 0.1f) s_duration = 1.0f;

        this->schedule(schedule_selector(MyPlayLayer::checkEffectEnd));
    }

    // Hàm này chỉ để kiểm tra khi nào thì dừng hiệu ứng
    void checkEffectEnd(float dt) {
        if (s_targetPitch <= 0.01f) {
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::checkEffectEnd));

            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }
        }
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        this->unschedule(schedule_selector(MyPlayLayer::checkEffectEnd));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_system) {
                FMOD::ChannelGroup* mg = nullptr;
                fmod->m_system->getMasterChannelGroup(&mg);
                if (mg) mg->setPitch(1.0f);
            }
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    void resetLevel() { this->resetOsuVars(); PlayLayer::resetLevel(); }
    void onQuit() { this->resetOsuVars(); PlayLayer::onQuit(); }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};