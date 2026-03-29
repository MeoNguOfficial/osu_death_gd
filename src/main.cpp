#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

// Sử dụng struct để thiết lập Priority cho Hook
// Giá trị âm (ví dụ -100) giúp mod chạy SAU các mod khác như Mega Hack
struct MyPriority {
    static inline int priority = -100;
};

class $modify(MyPlayLayer, PlayLayer) {
    // Áp dụng priority cho toàn bộ class modify
    static void onModify(SettingNode* node) {
        auto result = Interface::get()->init(Mod::get());
        if (!result) return;
        
        // Thiết lập ưu tiên chạy sau để tránh bị đè logic bởi Mega Hack
        (void)node;
    }

    struct Fields {
        bool m_isDead = false;
        float m_time = 0.0f;
        int m_actionTag = 1001;
        bool m_hasStarted = false; 
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontRun) {
        // Gọi init gốc trước
        if (!PlayLayer::init(level, useReplay, dontRun)) return false;
        
        // Reset trạng thái về ban đầu
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;
        m_fields->m_hasStarted = true; // Đánh dấu đã vào màn thành công
        
        this->cleanupOsuEffect();
        return true;
    }

    void applyOsuPitch() {
        // Chặn tuyệt đối nếu chưa chết hoặc chưa vào màn
        if (!m_fields->m_isDead || !m_fields->m_hasStarted) {
            this->stopActionByTag(m_fields->m_actionTag);
            return;
        }

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            double durationSetting = Mod::get()->getSettingValue<double>("fade-speed");
            float duration = static_cast<float>(durationSetting);
            if (duration <= 0.01f) duration = 1.0f;

            m_fields->m_time += 1.0f / 60.0f;
            float progress = 1.0f - (m_fields->m_time / duration);

            if (progress <= 0.0f) {
                progress = 0.0f;
                this->stopActionByTag(m_fields->m_actionTag);
            }

            // Công thức giảm Pitch theo bình phương để mượt hơn
            float finalPitch = progress * progress;
            fmod->m_backgroundMusicChannel->setPitch(finalPitch);
            
            // Ép Pitch cả SFX nếu muốn (giống file FadeOut.hpp)
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(finalPitch);
            }
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        // Chỉ kích hoạt khi thỏa mãn: đã bắt đầu màn chơi và chưa ở trạng thái chết
        if (m_fields->m_hasStarted && !m_fields->m_isDead) {
            m_fields->m_isDead = true;
            m_fields->m_time = 0.0f;

            // Dọn dẹp các action cũ nếu có
            this->stopActionByTag(m_fields->m_actionTag);

            // Tạo vòng lặp cập nhật Pitch qua Action để không bị đè bởi update()
            auto delay = CCDelayTime::create(1.0f / 60.0f);
            auto call = CCCallFunc::create(this, callfunc_selector(MyPlayLayer::applyOsuPitch));
            auto seq = CCSequence::create(delay, call, nullptr);
            auto repeat = CCRepeatForever::create(seq);
            repeat->setTag(m_fields->m_actionTag);
            
            this->runAction(repeat);
            log::info("Osu Death triggered hop le!");
        }
        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        // Khi nhấn R hoặc hồi sinh, lập tức trả nhạc về bình thường
        this->cleanupOsuEffect();
        PlayLayer::resetLevel();
    }

    void onQuit() {
        // Khi thoát ra menu, đảm bảo nhạc menu không bị méo
        this->cleanupOsuEffect();
        PlayLayer::onQuit();
    }

    void cleanupOsuEffect() {
        this->stopActionByTag(m_fields->m_actionTag);
        m_fields->m_isDead = false;
        m_fields->m_time = 0.0f;

        auto fmod = FMODAudioEngine::sharedEngine();
        if (fmod && fmod->m_backgroundMusicChannel) {
            fmod->m_backgroundMusicChannel->setPitch(1.0f);
            if (fmod->m_globalChannel) {
                fmod->m_globalChannel->setPitch(1.0f);
            }
        }
    }
};