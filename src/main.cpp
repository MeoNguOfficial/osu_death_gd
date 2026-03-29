#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

class $modify(MyPlayLayer, PlayLayer) {
    // Ưu tiên cao để chạy sau cùng, đảm bảo Pitch không bị mod khác reset
    static inline int priority = -100;

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        float m_initialCooldown = 0.0f;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_initialCooldown = 0.0f;
        
        return true;
    }

    void update(float dt) {
        PlayLayer::update(dt);
        
        // Luôn đếm thời gian từ lúc bắt đầu màn để chống trigger giả
        m_fields->m_initialCooldown += dt;

        // Nếu đang trong trạng thái chết, thực hiện giảm Pitch
        if (m_fields->m_isDead) {
            auto fmod = FMODAudioEngine::sharedEngine();
            if (fmod && fmod->m_backgroundMusicChannel) {
                double speedValue = Mod::get()->getSettingValue<double>("fade-speed");
                float duration = static_cast<float>(speedValue);
                if (duration <= 0.05f) duration = 1.0f; // Bảo vệ nếu setting lỗi

                m_fields->m_time += dt;
                float progress = 1.0f - (m_fields->m_time / duration);

                // Giới hạn progress từ 0.0 đến 1.0
                if (progress < 0.0f) progress = 0.0f;
                
                // Công thức Pitch giảm dần (progress * progress cho mượt)
                float finalPitch = progress * progress;
                
                fmod->m_backgroundMusicChannel->setPitch(finalPitch);
                if (fmod->m_globalChannel) {
                    fmod->m_globalChannel->setPitch(finalPitch);
                }
                
                // Log để Mèo kiểm tra trong Console
                if (progress > 0.0f) {
                    log::info("Osu Pitch: {:.2f}", finalPitch);
                }
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Gọi hàm gốc trước
        PlayLayer::destroyPlayer(player, obj);

        // Kiểm tra:
        // 1. Phải chơi được ít nhất 0.5s (chống lỗi auto-death lúc load)
        // 2. Player thực sự chết (m_isDead của game)
        // 3. Mod chưa ở trạng thái chết
        if (m_fields->m_initialCooldown > 0.5f && player->m_isDead && !m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;
            log::info("Osu Death Triggered thành công!");
        }
    }

    void resetLevel() {
        // Trả Pitch về 1.0 ngay lập tức khi nhấn R hoặc hồi sinh
        this->resetPitchManually();
        
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_initialCooldown = 0.0f;

        PlayLayer::resetLevel();
    }

    void onQuit() {
        this->resetPitchManually();
        PlayLayer::onQuit();
    }

    // Hàm phụ để reset nhạc sạch sẽ
    void resetPitchManually() {
        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod) {
            if (fmod->m_backgroundMusicChannel) fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) fmod->m_globalChannel->setPitch(1.0f);
        }
    }
};