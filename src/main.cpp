#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Sử dụng một biến global hoặc static để lưu trữ pitch hiện tại nhằm đồng bộ với FMOD hook
static float s_currentOsuPitch = 1.0f;
static bool s_isOsuEffectActive = false;

class $modify(MyFMOD, FMODAudioEngine) {
    void setBackgroundMusicPitch(float pitch) {
        // Nếu đang trong hiệu ứng chết, không cho phép game reset pitch về 1.0
        if (s_isOsuEffectActive) {
            FMODAudioEngine::setBackgroundMusicPitch(s_currentOsuPitch);
        } else {
            FMODAudioEngine::setBackgroundMusicPitch(pitch);
        }
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_isFading = false;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->cleanupOsuEffect();
        return true;
    }

    void onPlayerReallyDied() {
        // Kích hoạt trạng thái hiệu ứng
        s_isOsuEffectActive = true;
        m_fields->m_time = 0.0f;
        m_fields->m_isFading = true;

        this->stopActionByTag(m_fields->m_actionTag);

        // Sử dụng scheduleUpdate thay vì CCSequence để mượt hơn trên 2.2
        this->schedule(schedule_selector(MyPlayLayer::applyOsuPitchPtr));
        log::info("Osu Death triggered!");
    }

    // Wrapper để dùng với schedule
    void applyOsuPitchPtr(float dt) {
        this->applyOsuPitch();
    }

    void applyOsuPitch() {
        if (!m_fields->m_isFading) return;

        auto fmod = FMODAudioEngine::sharedEngine();
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = static_cast<float>(speedValue);
        if (duration <= 0.01f) duration = 1.0f;

        m_fields->m_time += 1.0f / 60.0f;
        float progress = 1.0f - (m_fields->m_time / duration);

        if (progress <= 0.0f) {
            progress = 0.0f;
            m_fields->m_isFading = false;
            s_isOsuEffectActive = false;
            this->unschedule(schedule_selector(MyPlayLayer::applyOsuPitchPtr));
        }

        // Công thức tính pitch (progress * progress tạo độ mượt)
        s_currentOsuPitch = progress; 

        if (fmod) {
            // Ép pitch thông qua hàm của Geode/GD thay vì can thiệp trực tiếp channel
            fmod->setBackgroundMusicPitch(s_currentOsuPitch);
            
            // QUAN TRỌNG: Ngăn chặn game tự động pause nhạc khi chết
            if (fmod->m_backgroundMusicChannel) {
                bool isPaused;
                fmod->m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && m_fields->m_isFading) {
                    fmod->m_backgroundMusicChannel->setPaused(false);
                }
            }
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
        this->unschedule(schedule_selector(MyPlayLayer::applyOsuPitchPtr));
        
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) fmod->setBackgroundMusicPitch(1.0f);
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