#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;

class $modify(MyFMOD, FMODAudioEngine) {
    // Hook hàm update để ép Pitch liên tục
    void update(float dt) {
        FMODAudioEngine::update(dt);
        if (s_isDeadEffect) {
            if (m_backgroundMusicChannel) {
                m_backgroundMusicChannel->setPitch(s_targetPitch);
                
                // CỰC KỲ QUAN TRỌNG: Nếu game cố tình pause nhạc, ta force resume ngay lập tức
                bool isPaused;
                m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && s_targetPitch > 0.05f) {
                    m_backgroundMusicChannel->setPaused(false);
                }
            }
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_targetPitch);
            }
        }
    }

    // Hook thêm hàm này để chặn các mod khác gọi lệnh dừng nhạc
    virtual void stopAllMusic() {
        if (s_isDeadEffect) return; // Từ chối dừng nhạc nếu đang chạy hiệu ứng Osu
        FMODAudioEngine::stopAllMusic();
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

        this->stopActionByTag(m_fields->m_osuActionTag);
        
        // Sử dụng một Action lặp lại để giảm pitch
        auto delay = CCDelayTime::create(0.01f); 
        auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::updateOsuFade));
        auto seq = CCSequence::create(delay, call, nullptr);
        this->runAction(CCRepeatForever::create(seq))->setTag(m_fields->m_osuActionTag);
    }

    void updateOsuFade() {
        if (!s_isDeadEffect) return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        
        // Giảm dần pitch
        s_targetPitch -= (0.01f / duration);

        if (s_targetPitch <= 0.01f) {
            this->resetOsuVars(); // Dừng hiệu ứng khi nhạc đã im lặng
            
            // Lúc này mới cho phép dừng nhạc thật sự
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