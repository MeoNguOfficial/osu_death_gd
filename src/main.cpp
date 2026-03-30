#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

static float s_currentOsuPitch = 1.0f;
static bool s_isOsuEffectActive = false;

class $modify(MyFMOD, FMODAudioEngine) {
    // Hook hàm update của engine để ép pitch mỗi frame khi đang chết
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (s_isOsuEffectActive && m_backgroundMusicChannel) {
            m_backgroundMusicChannel->setPitch(s_currentOsuPitch);
            
            // Đảm bảo nhạc không bị pause bởi game
            bool isPaused;
            m_backgroundMusicChannel->getPaused(&isPaused);
            if (isPaused) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        float m_time = 0.0f;
        bool m_isFading = false;
        // Tag để quản lý schedule
        bool m_scheduleEnabled = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->cleanupOsuEffect();
        return true;
    }

    void onPlayerReallyDied() {
        s_isOsuEffectActive = true;
        m_fields->m_time = 0.0f;
        m_fields->m_isFading = true;

        if (!m_fields->m_scheduleEnabled) {
            this->schedule(schedule_selector(MyPlayLayer::applyOsuPitch), 0.0f);
            m_fields->m_scheduleEnabled = true;
        }
        
        log::info("Osu Death Effect Started");
    }

    void applyOsuPitch(float dt) {
        if (!m_fields->m_isFading) return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        if (duration <= 0.01f) duration = 1.0f;

        m_fields->m_time += dt; // Sử dụng delta time thực tế cho mượt
        float progress = 1.0f - (m_fields->m_time / duration);

        if (progress <= 0.0f) {
            progress = 0.0f;
            m_fields->m_isFading = false;
            s_isOsuEffectActive = false; // Dừng việc ép pitch ở FMOD hook
            
            this->unschedule(schedule_selector(MyPlayLayer::applyOsuPitch));
            m_fields->m_scheduleEnabled = false;

            // Sau khi xong hiệu ứng thì mới cho dừng hẳn nhạc
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel) {
                    fmod->m_backgroundMusicChannel->setPaused(true);
                }
            }
        }

        s_currentOsuPitch = progress * progress; // Bình phương để pitch giảm trầm hơn
    }

    void resetLevel() {
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect() {
        s_isOsuEffectActive = false;
        s_currentOsuPitch = 1.0f;
        
        this->unschedule(schedule_selector(MyPlayLayer::applyOsuPitch));
        m_fields->m_scheduleEnabled = false;

        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
            }
        }
    }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
        
        if (auto playLayer = PlayLayer::get()) {
            // Caster an toàn sang class modify
            static_cast<MyPlayLayer*>(playLayer)->onPlayerReallyDied();
        }
    }
};