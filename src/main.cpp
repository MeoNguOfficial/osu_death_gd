#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;

// Chặn lệnh dừng nhạc gốc của game
class $modify(MyFMOD, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);
        if (s_isDeadEffect) {
            if (m_backgroundMusicChannel) {
                m_backgroundMusicChannel->setPitch(s_targetPitch);
                m_backgroundMusicChannel->setPaused(false); // Chống lại lệnh pause gốc
            }
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_targetPitch);
            }
        }
    }

    // Chặn game tắt nhạc khi chết
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return; 
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// Trong PlayLayer, thay vì dùng Action, hãy dùng schedule
void MyPlayLayer::onPlayerReallyDied() {
    if (s_isDeadEffect) return;
    s_isDeadEffect = true;
    s_targetPitch = 1.0f;

    // Thay vì chạy Action, hãy schedule update để mượt hơn theo FPS
    this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
}

void MyPlayLayer::updateOsuFade(float dt) {
    auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
    float duration = static_cast<float>(speedValue);

    // Giảm pitch dựa trên delta time thực tế của máy
    s_targetPitch -= dt / duration;

    if (s_targetPitch <= 0.01f) {
        s_targetPitch = 0.01f;
        s_isDeadEffect = false;
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
        
        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPaused(true);
        }
    }
}

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

        this->stopActionByTag(m_fields->m_osuActionTag);
        
        auto delay = CCDelayTime::create(0.01f); 
        auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::updateOsuFade));
        auto seq = CCSequence::create(delay, call, nullptr);
        auto repeat = CCRepeatForever::create(seq);
        repeat->setTag(m_fields->m_osuActionTag);

        this->runAction(repeat);
    }

    void updateOsuFade() {
        if (!s_isDeadEffect) return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        
        s_targetPitch -= (0.01f / duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->stopActionByTag(m_fields->m_osuActionTag);
            
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

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
            }
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(1.0f);
            }
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