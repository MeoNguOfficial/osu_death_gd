#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

static float s_targetPitch = 1.0f;
static bool s_isDeadEffect = false;
static float s_duration = 1.0f;

class $modify(MyFMODAudioEngine, FMODAudioEngine) {
    void update(float dt) {
        // Chạy gốc trước để GD thực hiện các hành vi mặc định
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        // Cập nhật Pitch dựa trên dt (Khớp speedhack)
        s_targetPitch -= (dt / s_duration);
        if (s_targetPitch < 0.01f) s_targetPitch = 0.01f;

        // BƯỚC 1: XÓA TWEEN CỦA GD (Chống Music bị câm)
        // Dựa trên file inline, ta dọn sạch container để GD không ghi đè Volume/Pitch nữa
        auto& musicTweens = this->getTweenContainer(AudioTargetType::MusicChannel);
        if (!musicTweens.empty()) {
            musicTweens.clear(); 
        }

        // BƯỚC 2: ÉP PITCH VÀO GLOBAL CHANNEL
        // Đây là tầng thấp nhất, GD khó can thiệp vào đây hơn
        if (m_globalChannel) {
            m_globalChannel->setPitch(s_targetPitch);
        }

        // BƯỚC 3: HỒI SINH BACKGROUND MUSIC
        if (m_backgroundMusicChannel) {
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            // Ép Volume luôn ở mức người dùng cài đặt, không cho GD fade về 0
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);
            
            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);
            if (isPaused && s_targetPitch > 0.1f) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    // Chặn tuyệt đối lệnh stop
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return;
        FMODAudioEngine::stopAllMusic(p0);
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        return true;
    }

    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;
        
        s_isDeadEffect = true;
        s_targetPitch = 1.0f;
        
        auto speed = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speed);
        if (s_duration < 0.1f) s_duration = 1.0f;

        // Resume nhạc ngay lập tức trước khi GD kịp dập
        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPaused(false);
                // Xóa tween ngay lúc này
                fmod->getTweenContainer(AudioTargetType::MusicChannel).clear();
            }
        }
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;

        if (auto fmod = FMODAudioEngine::sharedEngine()) {
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
            if (fmod->m_system) {
                FMOD::ChannelGroup* mg = nullptr;
                fmod->m_system->getMasterChannelGroup(&mg);
                if (mg) mg->setPitch(1.0f);
            }
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
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