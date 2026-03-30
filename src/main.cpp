#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Class quản lý hiệu ứng để tách biệt logic
class OsuDeathManager {
public:
    static OsuDeathManager* get() {
        static OsuDeathManager instance;
        return &instance;
    }

    float m_time = 0.f;
    float m_duration = 1.f;
    bool m_isFading = false;

    void start() {
        m_isFading = true;
        m_time = 0.f;
        
        auto speed = Mod::get()->getSettingValue<double>("fade-speed");
        m_duration = static_cast<float>(speed);
        if (m_duration <= 0.01f) m_duration = 1.f;
    }

    void stop() {
        m_isFading = false;
        m_time = 0.f;
    }
};

// Hook PlayLayer để reset hiệu ứng khi chơi lại hoặc thoát
class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        OsuDeathManager::get()->stop();
        return true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        OsuDeathManager::get()->stop();
    }

    void onQuit() {
        PlayLayer::onQuit();
        OsuDeathManager::get()->stop();
    }
};

// Hook PlayerObject để bắt khoảnh khắc "thực sự" chết
class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
        
        if (PlayLayer::get()) {
            OsuDeathManager::get()->start();
            log::info("Osu Death Started!");
        }
    }
};

// Hook FMODAudioEngine để ép Pitch và chặn lệnh Stop của game
class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        auto manager = OsuDeathManager::get();
        if (manager->m_isFading) {
            manager->m_time += dt;
            float progress = 1.f - std::min(manager->m_time / manager->m_duration, 1.f);
            
            if (progress <= 0.f) {
                manager->m_isFading = false;
                this->pauseBackgroundMusic();
                return;
            }

            float finalPitch = progress * progress;

            // Ép Pitch cho nhạc nền
            if (this->m_backgroundMusicChannel) {
                this->m_backgroundMusicChannel->setPitch(finalPitch);
                this->m_backgroundMusicChannel->setPaused(false); // Chống bị game pause
            }

            // Ép Pitch cho SFX (Global)
            if (this->m_globalChannel) {
                this->m_globalChannel->setPitch(finalPitch);
            }
        }
    }

    void stopBackgroundMusic() {
        if (OsuDeathManager::get()->m_isFading) return; // Chặn game dừng nhạc
        FMODAudioEngine::stopBackgroundMusic();
    }

    void pauseBackgroundMusic() {
        if (OsuDeathManager::get()->m_isFading) return; // Chặn game pause nhạc
        FMODAudioEngine::pauseBackgroundMusic();
    }
};