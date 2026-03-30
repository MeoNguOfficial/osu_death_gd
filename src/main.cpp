#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Sử dụng biến static để an toàn và truy cập nhanh
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;

class $modify(MyFMOD, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (s_isDeadEffect) {
            // Ép pitch cho nhạc nền (Music)
            if (m_backgroundMusicChannel) {
                m_backgroundMusicChannel->setPitch(s_targetPitch);
                
                // Cưỡng ép nhạc không bị pause khi đang chạy hiệu ứng
                bool isPaused;
                m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && s_targetPitch > 0.1f) {
                    m_backgroundMusicChannel->setPaused(false);
                }
            }
            
            // Ép pitch cho hiệu ứng âm thanh (SFX)
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_targetPitch);
            }
        }
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        int m_osuActionTag = 888;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->resetOsuVars();
        return true;
    }

    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        if (duration <= 0.05f) duration = 1.0f;

        // Dùng CCAction để chạy logic fade mà không làm đóng băng game
        this->stopActionByTag(m_fields->m_osuActionTag);
        
        auto delay = CCDelayTime::create(0.016f); // ~60fps
        auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::updateOsuFade));
        auto seq = CCSequence::create(delay, call, nullptr);
        auto repeat = CCRepeatForever::create(seq);
        repeat->setTag(m_fields->m_osuActionTag);

        this->runAction(repeat);
        log::info("Osu Death Started");
    }

    void updateOsuFade() {
        if (!s_isDeadEffect) return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        
        // Giảm pitch dần dần mỗi lần gọi (0.016s)
        float decrement = 0.016f / duration;
        s_targetPitch -= decrement;

        if (s_targetPitch <= 0.0f) {
            s_targetPitch = 0.0f;
            s_isDeadEffect = false;
            this->stopActionByTag(m_fields->m_osuActionTag);
            
            // Sau khi kết thúc hiệu ứng, cho phép game pause nhạc bình thường
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPaused(true);
            }
        }
    }

    void resetLevel() {
        this->resetOsuVars();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        this->resetOsuVars();
        PlayLayer::onQuit();
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        this->stopActionByTag(m_fields->m_osuActionTag);

        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
        if (auto playLayer = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(playLayer)->onPlayerReallyDied();
        }
    }
};