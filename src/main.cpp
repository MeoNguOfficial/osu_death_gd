#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        float m_ogMusicVolume = 1.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        auto fmod = FMODAudioEngine::sharedEngine();
        // Lưu lại volume gốc để trả về cho chuẩn
        m_fields->m_ogMusicVolume = fmod->m_backgroundMusicVolume;
        
        this->scheduleUpdate();
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);

        if (m_fields->m_isDead) {
            m_fields->m_time += dt;
            
            auto fmod = FMODAudioEngine::sharedEngine();
            if (!fmod) return;

            // Lấy Master Group để chỉnh Pitch
            FMOD::ChannelGroup* masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);

            if (masterGroup) {
                // MainProgress: Giảm dần về 0 trong khoảng 1 giây (hoặc chỉnh theo setting)
                float fadeSpeed = Mod::get()->getSettingValue<double>("fade-speed");
                float progress = std::max(1.0f - (m_fields->m_time * fadeSpeed), 0.0f);
                
                // 1. Chỉnh Pitch (Trầm dần)
                masterGroup->setPitch(progress);
                
                // 2. Chỉnh Volume (Nhỏ dần - cho giống cái FadeOut ví dụ của Mèo)
                // fmod->m_backgroundMusicChannel->setVolume(m_fields->m_ogMusicVolume * progress);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);
        if (!m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
        }
    }

    void resetLevel() {
        // Reset âm thanh về trạng thái ban đầu NGAY LẬP TỨC
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            FMOD::ChannelGroup* masterGroup;
            fmod->m_system->getMasterChannelGroup(&masterGroup);
            if (masterGroup) {
                masterGroup->setPitch(1.0f);
            }
        }

        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        PlayLayer::resetLevel();
    }
};