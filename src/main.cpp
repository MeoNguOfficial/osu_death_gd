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
        // Chạy update gốc trước
        FMODAudioEngine::update(dt);

        if (!s_isDeadEffect) return;

        // 1. ÉP PITCH TOÀN HỆ THỐNG (Dùng cho cả SFX và Music)
        if (m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(s_targetPitch);
            }
        }

        // 2. ÉP MUSIC CHANNEL (Chiến thuật cho 2.2081)
        if (m_backgroundMusicChannel) {
            // Ép Pitch riêng cho Music để chắc cú
            m_backgroundMusicChannel->setPitch(s_targetPitch);
            
            // ÉP VOLUME: Vì không hook được setBackgroundMusicVolume, ta ép trực tiếp ở đây
            // this->m_musicVolume là biến chứa âm lượng nhạc người dùng cài đặt
            m_backgroundMusicChannel->setVolume(this->m_musicVolume);

            // CHỐNG PAUSE: Nếu GD ra lệnh Pause khi chết, ta bật lại ngay
            bool isPaused = false;
            m_backgroundMusicChannel->getPaused(&isPaused);
            if (isPaused && s_targetPitch > 0.05f) {
                m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    // Hook hàm này thường an toàn hơn vì nó ít bị inline
    void stopAllMusic(bool p0) {
        if (s_isDeadEffect) return; // Chặn đứng lệnh dừng nhạc khi đang hẻo
        FMODAudioEngine::stopAllMusic(p0);
    }
};

class $modify(MyPlayLayer, PlayLayer) {
    void onPlayerReallyDied() {
        if (s_isDeadEffect) return;

        s_isDeadEffect = true;
        s_targetPitch = 1.0f;

        auto speedValue = Mod::get()->getSettingValue<double>("fade-speed");
        s_duration = static_cast<float>(speedValue);
        if (s_duration < 0.1f) s_duration = 1.0f;

        // "Mồi" một lệnh Resume ngay khi vừa chạm gai
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPaused(false);
            fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
        }

        this->schedule(schedule_selector(MyPlayLayer::updateOsuFade));
    }

    void updateOsuFade(float dt) {
        if (!s_isDeadEffect) return;

        s_targetPitch -= (dt / s_duration);

        if (s_targetPitch <= 0.01f) {
            s_targetPitch = 0.01f;
            s_isDeadEffect = false;
            this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

            // Chỉ khi hiệu ứng kết thúc mới cho nhạc dừng
            if (auto fmod = FMODAudioEngine::sharedEngine()) {
                if (fmod->m_backgroundMusicChannel)
                    fmod->m_backgroundMusicChannel->setPaused(true);
            }
        }
    }

    void resetOsuVars() {
        s_isDeadEffect = false;
        s_targetPitch = 1.0f;
        this->unschedule(schedule_selector(MyPlayLayer::updateOsuFade));

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_system) {
                FMOD::ChannelGroup* masterGroup = nullptr;
                fmod->m_system->getMasterChannelGroup(&masterGroup);
                if (masterGroup) masterGroup->setPitch(1.0f);
            }
            if (fmod->m_backgroundMusicChannel) {
                fmod->m_backgroundMusicChannel->setPitch(1.0f);
                fmod->m_backgroundMusicChannel->setVolume(fmod->m_musicVolume);
                fmod->m_backgroundMusicChannel->setPaused(false);
            }
        }
    }

    // Hook thêm resetLevel để chắc chắn âm thanh quay lại bình thường
    void resetLevel() {
        this->resetOsuVars();
        PlayLayer::resetLevel();
    }
};

class $modify(MyPlayerObject, PlayerObject) {
    void playDeathEffect() {
        PlayerObject::playDeathEffect();
        if (auto pl = PlayLayer::get()) {
            static_cast<MyPlayLayer*>(pl)->onPlayerReallyDied();
        }
    }
};