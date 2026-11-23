#ifndef NORTH_ISLAND_CLIENT_MODELS_PETS_PET_TEAM_H
#define NORTH_ISLAND_CLIENT_MODELS_PETS_PET_TEAM_H

#include "client/models/pets/Pet.h"
#include "formats/FormulaData.h"
#include "formats/ItemTable.h"
#include "formats/NpcTable.h"
#include "protocol/PetRecord.h"

#include <array>
#include <optional>
#include <vector>

namespace client {

    /// @brief The own player's pets in places 1..4 (original Player.FPets) and which one fights (FActivePetId / FActivePetSlot).
    class PetTeam {
        public:
            static constexpr int kPlaces = 4;
            static constexpr int kExpressType = 1; // Npc.dat type 1: the pets that can be sent by express

            /// @brief Creates an empty team.
            /// @param formula Formula data for HP/SP maxima.
            /// @param npcs Npc table giving the pets' kinds.
            /// @param items Item table giving equipment slots.
            PetTeam(
                const formats::FormulaData& formula,
                const formats::NpcTable& npcs,
                const formats::ItemTable& items
            )
                : m_formula(formula),
                  m_npcs(npcs),
                  m_items(items) {}

            /// @brief Sets the whole team from the server list; kinds already in the team are skipped.
            /// @details Original: packet 15/8 (RevInitFNpc, SetPetAtSlot).
            /// @param records Pet records sent by the server.
            void setList(const std::vector<protocol::PetRecord>& records);

            /// @brief Adds a pet in the first free place with full HP and SP.
            /// @details Original: packet 15/1 (AddPetToTeam).
            /// @param record The joining pet.
            /// @return The place taken, or 0 when none.
            int join(const protocol::PetRecord& record);

            /// @brief Puts a pet back into a place with its kind and state as it slept; the place keeps its join order.
            /// @details Original: packet 62/44 (a pet back from a bed of a house).
            /// @param place Place 1..4.
            /// @param record The pet record.
            /// @return False for an unknown kind or invalid place.
            bool put(int place, const protocol::PetRecord& record);

            /// @brief Removes the pet of a place; the later ones move up the order.
            /// @details Original: packet 15/2 (RemovePetFromTeam).
            /// @param place Place 1..4.
            void remove(int place);

            /// @brief Empties the team and clears the active pet and city battle marks.
            void clear();

            /// @brief Sets the fighting pet by its kind.
            /// @details Original: packets 19/1 and 19/4 set it, 19/2 clears it (0).
            /// @param npcId Kind of the fighting pet, 0 for none.
            void setActive(int npcId);

            /// @brief Gets the place of the fighting pet.
            /// @return The place 1..4, or 0 for none.
            int activePlace() const { return m_activePlace; }

            /// @brief Replaces worn item ids in a range by one id.
            /// @details Original: RevertItemVariantsTo20747.
            /// @param first First id of the range.
            /// @param last Last id of the range.
            /// @param to Id the worn items take.
            void replaceWornIds(int first, int last, int to);

            /// @brief Marks a pet as dead in a city battle (it cannot join again, be dismissed or traded until it is back).
            /// @details Original: packet 19/6 (FCityBattleDeadPets).
            /// @param place Place 1..4.
            /// @param dead True when the pet died.
            void setCityBattleDead(int place, bool dead);

            /// @brief Tells whether a pet died in a city battle.
            /// @param place Place 1..4.
            /// @return True when marked dead.
            bool cityBattleDead(int place) const { return place >= 1 && place <= kPlaces && m_cityBattleDead[static_cast<std::size_t>(place)]; }

            /// @brief Sets the pet the own player rides.
            /// @details Original: packets 15/16 and 15/17 (FRidePetSlot).
            /// @param place Place 1..4, 0 for none.
            void setRiding(int place) { m_ridingPlace = place; }

            /// @brief Gets the place of the ridden pet.
            /// @return The place, 0 for none.
            int ridingPlace() const { return m_ridingPlace; }

            /// @brief Applies an attribute change to a pet (HP, SP, level, experience, amity, skills, bonuses).
            /// @details Original: packet 8/2, SetHumanAttr on a pet.
            /// @param place Place 1..4.
            /// @param attribute SetHumanAttr attribute number.
            /// @param value New value.
            /// @param extra Extra value (the skill id for skill attributes).
            void setAttribute(int place, int attribute, std::int32_t value, std::int32_t extra);

            /// @brief Wears an item in the slot Item.dat gives it (original SetPetEquipItem).
            /// @param place Place 1..4.
            /// @param item The item to wear.
            /// @return False for an unknown item or a slot beyond 1..6.
            bool equip(int place, const protocol::ItemInstance& item);

            /// @brief Empties an equipment slot of a pet (original ClearEquipSlot).
            /// @param place Place 1..4.
            /// @param equipSlot Equipment slot 1..6.
            void clearEquipment(int place, int equipSlot);

            /// @brief Puts a bag item onto a pet and returns what it wore there (original EquipPetFromBag's result).
            /// @param place Place 1..4.
            /// @param item The item going onto the pet.
            /// @return The item that was worn in that slot, empty when nothing.
            protocol::ItemInstance swapEquipment(int place, const protocol::ItemInstance& item);

            /// @brief Gets the pet of a place.
            /// @param place Place 1..4.
            /// @return The pet slot (empty when no pet).
            const std::optional<Pet>& at(int place) const;

            /// @brief Gets the pet of a place for change.
            /// @param place Place 1..4.
            /// @return The pet slot (empty when no pet).
            std::optional<Pet>& at(int place);

            /// @brief Finds the place of the pet with a join order (original FindPetSlotByOrder).
            /// @param order Join order 1..4.
            /// @return The place, or 0 when no pet has that order.
            int placeByOrder(int order) const;

            /// @brief Finds the place of a pet by its kind.
            /// @param npcId Npc kind.
            /// @return The place, or 0 when not in the team.
            int placeOf(int npcId) const;

            /// @brief Counts the pets in the team.
            /// @return The number of pets.
            int count() const;

            /// @brief Recomputes the most HP and SP from level, constitution and wisdom (original RecalcPetStats).
            /// @param pet The pet to update.
            void recalc(Pet& pet) const;

            /// @brief Recomputes only the most HP and SP.
            /// @param pet The pet to update.
            void recalcMaxima(Pet& pet) const;

            /// @brief Builds a pet from a server record (its kind from Npc.dat).
            /// @param record The server record.
            /// @return The pet, or nothing for an unknown kind.
            std::optional<Pet> fromRecord(const protocol::PetRecord& record) const;

        private:
            /// @brief Wears an item in its slot (original THuman.EquipItem on a pet).
            /// @param pet The pet.
            /// @param item The item to wear.
            /// @return False when the item has no slot of the six.
            bool wear(Pet& pet, const protocol::ItemInstance& item) const;

            const formats::FormulaData& m_formula;
            const formats::NpcTable& m_npcs;
            const formats::ItemTable& m_items;
            std::array<std::optional<Pet>, kPlaces + 1> m_places;
            int m_activePlace = 0;
            std::array<bool, kPlaces + 1> m_cityBattleDead{};
            int m_ridingPlace = 0;
    };

}

#endif
