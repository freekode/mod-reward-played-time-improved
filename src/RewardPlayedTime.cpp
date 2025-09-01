/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

bool modEnabled;
bool modAnnounce;
uint32 rewardIntervalMinutes;

// std::vector<std::pair<uint32, uint32>> RewardItems;
std::vector<uint32> items;
std::unordered_map<ObjectGuid, uint32> timers;


class mod_reward_time_played_conf : public WorldScript
{
public:
    mod_reward_time_played_conf() : WorldScript("mod_reward_time_played_conf", {}) { }

    // Load Configuration Settings
    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        modEnabled = sConfigMgr->GetOption<bool>("RewardPlayedTime.Enable", true);
        modAnnounce = sConfigMgr->GetOption<bool>("RewardPlayedTime.Announce", true);
        rewardIntervalMinutes = sConfigMgr->GetOption<uint32>("RewardPlayedTime.RewardInterval", 3600);

        // Get reward list
        std::string itemList = sConfigMgr->GetOption<std::string>("RewardPlayedTime.RewardItems", "");        
        std::stringstream ss(itemList);
        std::string token;
        items.clear();
        while (std::getline(ss, token, ','))
        {
            items.push_back(std::stoul(token));
        }
    }
};

class RewardPlayedTime : public PlayerScript
{
public:
    RewardPlayedTime() : PlayerScript("RewardPlayedTime") { }

    std::string mail_subject = "RewardPlayedTime";
    std::string mail_body = "Congratulations! For your hard work you got a reward, keep it up!";

    void OnPlayerLogin(Player* player) override
    {
        if (!modEnabled)
        {
            return;
        }
        if (modAnnounce)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server is running the |cff4CFF00Reward Time Played Improved |rmodule.");
        }

        timers[player->GetGUID()] = 0;
    }

    void OnPlayerBeforeUpdate(Player* player, uint32 p_time) override
    {
        if (!modEnabled)
        {
            return;
        }

        uint32 intervalMs = rewardIntervalMinutes * SECOND * IN_MILLISECONDS;

        ObjectGuid guid = player->GetGUID();

        auto player_timer = timers.find(guid);
        if (player_timer == timers.end())
        {
            return; // player not tracked
        }

        if (player->isAFK())
        {
            return;
        }

        player_timer->second += p_time;
        if (player_timer->second < intervalMs)
        {
            return;
        }

        player_timer->second = 0; // reset timer

        if (items.empty())
        {
            LOG_WARN("module", "[RewardPlayedTime]: RewardItems list could not be parsed. Check your config!");
            return; // no items configured
        }

        int32 roll = urand(0, items.size() - 1);
        uint32 rewardItemId = items[roll];

        SendRewardToPlayer(player, rewardItemId, 1);
    }

    void SendRewardToPlayer(Player* receiver, uint32 itemId, uint32 count)
    {
        ItemTemplate const* item_template = sObjectMgr->GetItemTemplate(itemId);
        if (!item_template)
        {
            LOG_ERROR("module", "[RewardPlayedTime]: The itemId is invalid: {}", itemId);
            return;
        }
        if (count < 1 || (item_template->MaxCount > 0 && count > uint32(item_template->MaxCount)))
        {
            LOG_ERROR("module", "[RewardPlayedTime]: The item count is invalid: {} : {}", itemId, count);
            return;
        }

        std::ostringstream item_quality_string;
        item_quality_string << std::hex << ItemQualityColors[item_template->Quality];

        ChatHandler(receiver->GetSession()).PSendSysMessage("Congratulations! For your hard work you got a reward!");

        if (receiver->IsInWorld() && receiver->AddItem(itemId, 1))
        {
            // // Show item link in chat
            // ChatHandler(receiver->GetSession()).PSendSysMessage("You got - |c{}|Hitem:{}:0:0:0:0:0:0:0:0:0|h[{}]|h|r",
            //     item_quality_string.str(),
            //     item_template->ItemId,
            //     item_template->Name1);
            return;
        }

        ChatHandler(receiver->GetSession()).PSendSysMessage("Oh no! But don't worry, your item is send to your mailbox.");

        MailDraft draft(mail_subject, mail_body);
        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();

        if (Item* item = Item::CreateItem(itemId, count))
        {
            item->SaveToDB(trans);
            draft.AddItem(item);
        }
        draft.SendMailTo(trans, MailReceiver(receiver), MailSender(receiver));

        CharacterDatabase.CommitTransaction(trans);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!modEnabled) {
            return;
        }

        timers.erase(player->GetGUID());
    }
};

void AddRewardPlayedTimeScripts()
{
    new mod_reward_time_played_conf();
    new RewardPlayedTime();
}
