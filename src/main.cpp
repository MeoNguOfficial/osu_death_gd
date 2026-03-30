#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

// Các biến static dùng chung để điều khiển hiệu ứng
static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;

// 1. Hook FMOD để ép Pitch không bị game ghi đè khi chết
class $modify(MyFMOD, FMODAudioEngine) {
    void update(float dt) {
        FMODAudioEngine::update(dt);
        
        if (s_isDeadEffect) {
            // Ép Pitch cho nhạc nền
            if (m_backgroundMusicChannel) {
                m_backgroundMusicChannel->setPitch(s_targetPitch);
                // Quan trọng: Chống lại lệnh pause tự động của game
                m_backgroundMusicChannel->setPaused(false); 
            }
            // Ép Pitch cho hiệu ứng âm thanh (SFX)
            if (m_globalChannel) {
                m_globalChannel->setPitch(s_targetPitch);
            }
        }
    }

    // Ngăn game tắt nhạc ngay lập tức khi chết
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return; 
        FMODAudioEngine::stopAllMusic(p0);
    }
};

// 2. Hook PlayLayer để xử lý logic Fade Out
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        // Không cần tag nữa vì dùng schedule sẽ mượt hơn action
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        this->resetOsuVars();
        return true;
    }

    // Hàm được gọi khi người chơi thực sự nổ
    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;

        // Lấy thời gian fade từ setting (mặc định 1.0 nếu lỗi)
        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05) ? 1.0f : static_cast<float>(speedValue);

        // Sử dụng schedule để chạy update mỗi frame (mượt hơn CCDelayTime)
        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    // Logic giảm Pitch theo thời gian
    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        float duration = (speedValue <= 0.05) ? 1.0f : static_cast<float>(speedValue);
        
        // Giảm pitch dựa trên delta time (thời gian thực giữa 2 frame)
        s_targetPitch -= (dt / duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            
            // Dừng việc update
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));
            
            // Sau khi fade xong thì mới cho nhạc dừng hẳn
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
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};

// 3. Hook PlayerObject để bắt sự kiện chết
class $modify(MyPlayerObject, PlayerObject) {
    void playerDestroyed(bool p0) {
        PlayerObject::playerDestroyed(p0);
        
        if (auto playLayer = PlayLayer::get()) {
            // Ép kiểu về MyPlayLayer để gọi hàm custom
            static_cast<MyPlayLayer*>(playLayer)->onPlayerReallyDied();
        }
    }
};