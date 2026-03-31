#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Các biến điều khiển trạng thái hiệu ứng
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;

// 1. Hook FMOD: Trái tim của bản mod, dùng để ép engine âm thanh
class $modify(MyFMOD, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);

        if (s_isDeadEffect) {
            // Xử lý nhạc nền (Background Music)
            if (m_backgroundMusicChannel) {
                // Ép Pitch giảm dần theo biến static
                m_backgroundMusicChannel->setPitch(s_targetPitch);
                
                // Ép Volume luôn ở mức 1.0 (Zikko dùng cách này để nhạc không bị tắt đột ngột)
                m_backgroundMusicChannel->setVolume(1.0f); 

                // CHỐNG PAUSE: Nếu game cố tình dừng nhạc khi chết, ta Resume nó lại ngay
                bool isPaused;
                m_backgroundMusicChannel->getPaused(&isPaused);
                if (isPaused && s_targetPitch > 0.05f) {
                    m_backgroundMusicChannel->setPaused(false);
                }
            }

            // Xử lý SFX (Tiếng nổ, tiếng click...)
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_targetPitch);
            }
        }
    }

    // Chặn hàm dừng nhạc mặc định của GD khi s_isDeadEffect đang chạy
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return; 
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// 2. Hook PlayLayer: Quản lý logic thời gian (Timeline) của cú ngã
class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->resetOsuVars();
        return true;
    }

    void onPlayerReallyDied() {
        if (s_isDeadEffect) return; // Tránh gọi trùng lặp

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05) ? 1.0f : static_cast<float>(speedValue);

        // Bắt đầu vòng lặp giảm pitch mỗi frame
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05) ? 1.0f : static_cast<float>(speedValue);

        // Giảm pitch dựa trên thời gian thực (dt) giúp hiệu ứng mượt ở mọi mức FPS
        s_targetPitch -= (dt / duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Hiệu ứng kết thúc, bây giờ mới thực sự cho nhạc dừng
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPaused(true);
            }
        }
    }

    // Reset lại mọi thứ khi chơi lại hoặc thoát
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
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};

// 3. Hook PlayerObject: Điểm kích hoạt (Trigger)
class $modify(MyPlayerObject, PlayerObject) {
    // Chúng ta dùng playDeathEffect vì nó chuẩn xác nhất cho âm thanh
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        
        if (auto playLayer = PlayLayer::get()) {
            // Gọi trực tiếp thông qua cast để đảm bảo không lỗi tham chiếu
            static_cast<MyPlayLayer*>(playLayer)->onPlayerReallyDied();
        }
    }
    
    // Giữ lại playerDestroyed nhưng không cần gọi thêm onPlayerReallyDied ở đây
    // để tránh việc trigger 2 lần gây lag hoặc lỗi pitch
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
    }
};