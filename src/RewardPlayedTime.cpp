/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"

class RewardPlayedTime : public PlayerScript
{
public:
    RewardPlayedTime() : PlayerScript("RewardPlayedTime") { }

    // std::vector<std::pair<uint32, uint32>> RewardItems;
    std::unordered_map<ObjectGuid, uint32> timers;
    std::string mail_subject = "RewardPlayedTime";
    std::string mail_body = "Congratulations! For your hard work you got a reward, keep it up!";

    void OnPlayerLogin(Player* player) override
    {
        if (!sConfigMgr->GetOption<bool>("RewardPlayedTime.Enable", true))
        {
            return;
        }

        if (sConfigMgr->GetOption<bool>("RewardPlayedTime.Announce", true) )
        {
            ChatHandler(player->GetSession()).PSendSysMessage("This server is running the |cff4CFF00Reward Time Played Improved |rmodule.");
        }

        timers[player->GetGUID()] = 0;
    }

    void OnPlayerBeforeUpdate(Player* player, uint32 p_time) override
    {
        if (!sConfigMgr->GetOption<bool>("RewardPlayedTime.Enable", true))
        {
            return;
        }

        uint32 rewardIntervalMinutes = sConfigMgr->GetOption<uint32>("RewardPlayedTime.RewardInterval", 3600);
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
        ChatHandler(player->GetSession()).PSendSysMessage("ping");

        // Get reward list
        std::string itemList = sConfigMgr->GetOption<std::string>("RewardPlayedTime.RewardItems", "");
        std::vector<uint32> items;

        std::stringstream ss(itemList);
        std::string token;
        while (std::getline(ss, token, ','))
        {
            items.push_back(std::stoul(token));
        }

        if (items.empty())
        {
            LOG_WARN("module", "[RewardPlayedTime]: RewardItems list could not be parsed. Check your config!");
            return; // no items configured
        }

        int32 roll = urand(0, items.size() - 1);
        uint32 rewardItemId = items[roll];

        LOG_INFO("module", "[RewardPlayedTime]: rr {}, item: {}", roll, rewardItemId);

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

        ChatHandler(receiver->GetSession()).PSendSysMessage("Oh oh! Don't worry, your item is send to your mailbox.");

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
        if (!sConfigMgr->GetOption<bool>("RewardPlayedTime.Enable", true)) {
            return;
        }

        timers.erase(player->GetGUID());
    }
};

void AddRewardPlayedTimeScripts()
{
    new RewardPlayedTime();
}
