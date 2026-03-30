#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

static float s_currentOsuPitch = 1.0f;
static bool s_isOsuEffectActive = false;

class $modify(MyFMOD, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (s_isOsuEffectActive) {
            // Ép pitch cho nhạc nền
            if (m_backgroundMusicChannel) {
                m_backgroundMusicChannel->setPitch(s_currentOsuPitch);
                
                // Ngăn chặn việc nhạc bị pause đột ngột bởi Mod khác hoặc Game
                bool isPaused;
                m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && s_currentOsuPitch > 0.01f) {
                    m_backgroundMusicChannel->setPaused(false);
                }
            }

            // Ép pitch cho toàn bộ SFX (tiếng nổ, v.v.)
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_currentOsuPitch);
            }
        }
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        float m_time = 0.0f;
        bool m_isFading = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->cleanupOsuEffect();
        return true;
    }

    void onPlayerReallyDied() {
        // Nếu đã đang fade rồi thì không chạy lại tránh trùng lặp
        if (m_fields->m_isFading) return;

        s_isOsuEffectActive = true;
        s_currentOsuPitch = 1.0f;
        m_fields->m_time = 0.0f;
        m_fields->m_isFading = true;

        // Dùng scheduleUpdate của CCNode để chạy mỗi frame
        this->scheduleUpdate();
        
        log::info("Osu Death Effect Started");
    }

    void update(float dt) override {
        PlayLayer::update(dt);

        if (m_fields->m_isFading) {
            auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
            float duration = static_cast<float>(speedValue);
            if (duration <= 0.05f) duration = 1.0f;

            m_fields->m_time += dt;
            float progress = 1.0f - (m_fields->m_time / duration);

            if (progress <= 0.0f) {
                progress = 0.0f;
                m_fields->m_isFading = false;
                s_isOsuEffectActive = false;
                this->unscheduleUpdate();

                // Dừng hẳn nhạc khi kết thúc fade
                auto fmod = FMODAudioEngine::sharedEngine();
                if (fmod && fmod->m_backgroundMusicChannel) {
                    fmod->m_backgroundMusicChannel->setPaused(true);
                }
            }

            // Gán giá trị pitch để FMOD Hook sử dụng
            s_currentOsuPitch = std::clamp(progress * progress, 0.0f, 1.0f);
        }
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
        m_fields->m_isFading = false;
        this->unscheduleUpdate();

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