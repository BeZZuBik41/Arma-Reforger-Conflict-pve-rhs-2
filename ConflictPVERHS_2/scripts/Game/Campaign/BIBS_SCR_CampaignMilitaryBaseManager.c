modded class SCR_CampaignMilitaryBaseManager
{
    override void InitializeBases(notnull array<SCR_CampaignMilitaryBaseComponent> selectedHQs, bool randomizeSupplies)
    {
        array<SCR_CampaignMilitaryBaseComponent> basesSorted = {};
        SCR_CampaignMilitaryBaseComponent baseCheckedAgainst;
        vector originHQ1 = selectedHQs[0].GetOwner().GetOrigin();
        vector originHQ2 = selectedHQs[1].GetOwner().GetOrigin();
        float distanceToHQ;
        bool indexFound;
        int callsignIndex;
        array<int> allCallsignIndexes = {};
        array<int> callsignIndexesBLUFOR = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.BLUFOR).GetBaseCallsignIndexes();
        SCR_CampaignFaction factionOPFOR = m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.OPFOR);

        // Собираем все допустимые индексы позывных
        foreach (int indexBLUFOR : callsignIndexesBLUFOR)
        {
            if (factionOPFOR.GetBaseCallsignByIndex(indexBLUFOR))
                allCallsignIndexes.Insert(indexBLUFOR);
            else
                Print("Предупреждение: Нет соответствующего позывного OPFOR для индекса " + indexBLUFOR);
        }

        if (allCallsignIndexes.IsEmpty())
            Print("Ошибка: allCallsignIndexes пуст! Используются индексы баз для позывных.");

        int callsignsCount = allCallsignIndexes.Count();
        Faction defaultFaction;
        BaseRadioComponent radio;
        BaseTransceiver tsv;

        Print("Инициализация баз. m_aBases count: " + m_aBases.Count() + ", allCallsignIndexes count: " + allCallsignIndexes.Count());

        foreach (int iBase, SCR_CampaignMilitaryBaseComponent campaignBase : m_aBases)
        {
            if (!campaignBase || !campaignBase.IsInitialized())
            {
                Print("Предупреждение: Пропуск неинициализированной базы с индексом " + iBase);
                continue;
            }

            defaultFaction = campaignBase.GetFaction(true);

            // Применяем фракцию по умолчанию
            if (!campaignBase.GetFaction())
            {
                if (defaultFaction)
                    campaignBase.SetFaction(defaultFaction);
                else
                    campaignBase.SetFaction(m_Campaign.GetFactionByEnum(SCR_ECampaignFaction.INDFOR));
            }

            // Назначаем позывной
            if (campaignBase.GetType() == SCR_ECampaignBaseType.BASE)
            {
                if (allCallsignIndexes.IsEmpty())
                {
                    Print("Предупреждение: Нет доступных позывных для базы " + campaignBase.GetOwner().GetName() + ", используется индекс базы: " + iBase);
                    campaignBase.SetCallsignIndex(iBase); // Используем индекс базы как позывной
                }
                else
                {
                    callsignIndex = allCallsignIndexes.GetRandomIndex();
                    campaignBase.SetCallsignIndex(allCallsignIndexes[callsignIndex]);
                    allCallsignIndexes.Remove(callsignIndex);
                }
            }
            else
            {
                campaignBase.SetCallsignIndex(callsignsCount + iBase);
            }

            campaignBase.RefreshTasks();

            // Сортируем базы по расстоянию до штаба
            if (randomizeSupplies && campaignBase.GetType() == SCR_ECampaignBaseType.BASE)
            {
                indexFound = false;
                distanceToHQ = vector.DistanceSqXZ(originHQ1, campaignBase.GetOwner().GetOrigin());

                for (int i = 0, count = basesSorted.Count(); i < count; i++)
                {
                    baseCheckedAgainst = basesSorted[i];

                    if (distanceToHQ < vector.DistanceSqXZ(originHQ1, baseCheckedAgainst.GetOwner().GetOrigin()))
                    {
                        basesSorted.InsertAt(campaignBase, i);
                        indexFound = true;
                        break;
                    }
                }

                if (!indexFound)
                    basesSorted.Insert(campaignBase);
            }
        }

        if (randomizeSupplies)
            AddRandomSupplies(basesSorted, selectedHQs);
    }
}