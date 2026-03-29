#include <Geode/Geode.hpp>

// Include các header cần thiết
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/FMODAudioEngine.hpp>

using namespace geode::prelude;

// --- PHẦN 1: NÚT BẤM Ở MENU ---
class $modify(MyMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto myButton = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_likeBtn_001.png"),
            this,
            menu_selector(MyMenuLayer::onMyButton)
        );

        auto menu = this->getChildByID("bottom-menu");
        if (menu) {
            menu->addChild(myButton);
            myButton->setID("my-button"_spr);
            menu->updateLayout();
        }

        return true;
    }

    void onMyButton(CCObject*) {
        FLAlertLayer::create("Geode", "Hello from my custom mod!", "OK")->show();
    }
}; // <--- PHẢI CÓ DẤU CHẤM PHẨY Ở ĐÂY

// --- PHẦN 2: HIỆU ỨNG OSU DEATH ---
class $modify(MyPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        PlayLayer::destroyPlayer(player, obj);

        // Làm méo nhạc khi hẻo
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine && engine->m_system) { // Thêm check an toàn để tránh crash
            FMOD::ChannelGroup* masterGroup = nullptr;
            if (engine->m_system->getMasterChannelGroup(&masterGroup) == FMOD_OK && masterGroup) {
                masterGroup->setPitch(0.35f);
            }
        }
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        
        // Trả nhạc về bình thường khi hồi sinh
        auto engine = FMODAudioEngine::sharedEngine();
        if (engine && engine->m_system) {
            FMOD::ChannelGroup* masterGroup = nullptr;
            if (engine->m_system->getMasterChannelGroup(&masterGroup) == FMOD_OK && masterGroup) {
                masterGroup->setPitch(1.0f);
            }
        }
    }
}; // <--- PHẢI CÓ DẤU CHẤM PHẨY Ở ĐÂY