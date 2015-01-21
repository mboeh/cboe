
#include <cstdio>
#include <cstring>

#include "boe.global.h"

#include "classes.h"

#include "boe.graphics.h"
#include "boe.text.h"
#include "boe.items.h"
#include "boe.specials.h"
#include "boe.party.h"
#include "boe.locutils.h"
#include "boe.newgraph.h"
#include "boe.itemdata.h"
#include "boe.infodlg.h"
#include "soundtool.hpp"
#include "boe.monster.h"
#include "boe.main.h"
#include "graphtool.hpp"
#include "mathutil.hpp"
#include "dlogutil.hpp"
#include "restypes.hpp"
#include "message.hpp"
#include <array>
#include <boost/lexical_cast.hpp>
#include "winutil.hpp"

extern short stat_window,which_combat_type,current_pc;
extern eGameMode overall_mode;
extern sf::RenderWindow mainPtr;
extern bool boom_anim_active;
extern rectangle d_rects[80];
extern short d_rect_index[80];

extern bool map_visible,diff_depth_ok;
extern sf::RenderWindow mini_map;
extern sf::Texture pc_gworld;
extern cUniverse univ;

extern const std::multiset<eItemType> equippable;
extern const std::multiset<eItemType> num_hands_to_use;
extern std::map<const eItemType, const short> excluding_types;

short selected,item_max = 0;

bool GTP(short item_num) {
	cItem item;
	
	item = get_stored_item(item_num);
	return univ.party.give_item(item,true);
}
bool silent_GTP(short item_num) {
	cItem item;
	
	item = get_stored_item(item_num);
	return univ.party.give_item(item,false);
}
void give_gold(short amount,bool print_result) {
	if(amount < 0) return;
	univ.party.gold += amount;
	if(print_result)
		put_pc_screen();
}

bool take_gold(short amount,bool print_result) {
	if(univ.party.gold < amount)
		return false;
	univ.party.gold += amount;
	if(print_result)
		put_pc_screen();
	return true;
}

void give_food(short amount,bool print_result) {
	if(amount < 0) return;
	univ.party.food = univ.party.food + amount;
	if(print_result)
		put_pc_screen();
}

short take_food(short amount,bool print_result) {
	short diff;
	
	diff = amount - univ.party.food;
	if(diff > 0) {
		univ.party.food = 0;
		if(print_result)
			put_pc_screen();
		return diff;
	}
	
	univ.party.food = univ.party.food - amount;
	if(print_result)
		put_pc_screen();
	return 0;
}

void enchant_weapon(short pc_num,short item_hit,short enchant_type,short new_val) {
	if(univ.party[pc_num].items[item_hit].magic ||
		(univ.party[pc_num].items[item_hit].ability != eItemAbil::NONE))
		return;
	univ.party[pc_num].items[item_hit].magic = true;
	univ.party[pc_num].items[item_hit].enchanted = true;
	std::string store_name = univ.party[pc_num].items[item_hit].full_name;
	switch(enchant_type) {
		case 0:
			store_name += " (+1)";
			univ.party[pc_num].items[item_hit].bonus++;
			univ.party[pc_num].items[item_hit].value = new_val;
			break;
		case 1:
			store_name += " (+2)";
			univ.party[pc_num].items[item_hit].bonus += 2;
			univ.party[pc_num].items[item_hit].value = new_val;
			break;
		case 2:
			store_name += " (+3)";
			univ.party[pc_num].items[item_hit].bonus += 3;
			univ.party[pc_num].items[item_hit].value = new_val;
			break;
		case 3:
			store_name += " (F)";
			univ.party[pc_num].items[item_hit].ability = eItemAbil::CAST_SPELL;
			univ.party[pc_num].items[item_hit].abil_data[0] = 5;
			univ.party[pc_num].items[item_hit].abil_data[1] = int(eSpell::FLAME);
			univ.party[pc_num].items[item_hit].charges = 8;
			break;
		case 4:
			store_name += " (F!)";
			univ.party[pc_num].items[item_hit].value = new_val;
			univ.party[pc_num].items[item_hit].ability = eItemAbil::DAMAGING_WEAPON;
			univ.party[pc_num].items[item_hit].abil_data[0] = 5;
			univ.party[pc_num].items[item_hit].abil_data[1] = int(eDamageType::FIRE);
			break;
		case 5:
			store_name += " (+5)";
			univ.party[pc_num].items[item_hit].value = new_val;
			univ.party[pc_num].items[item_hit].bonus += 5;
			break;
		case 6:
			store_name += " (B)";
			univ.party[pc_num].items[item_hit].bonus++;
			univ.party[pc_num].items[item_hit].ability = eItemAbil::AFFECT_STATUS;
			univ.party[pc_num].items[item_hit].abil_data[0] = 5;
			univ.party[pc_num].items[item_hit].abil_data[1] = int(eStatus::BLESS_CURSE);
			univ.party[pc_num].items[item_hit].magic_use_type = 0;
			univ.party[pc_num].items[item_hit].charges = 8;
			break;
	}
	if(univ.party[pc_num].items[item_hit].value > 15000)
		univ.party[pc_num].items[item_hit].value = 15000;
	if(univ.party[pc_num].items[item_hit].value < 0)
		univ.party[pc_num].items[item_hit].value = 15000;
	univ.party[pc_num].items[item_hit].full_name = store_name;
}

void equip_item(short pc_num,short item_num) {
	unsigned short num_equipped_of_this_type = 0;
	unsigned short num_hands_occupied = 0;
	short i;
	short equip_item_type = 0;
	
	if(overall_mode == MODE_COMBAT && univ.party[pc_num].items[item_num].variety == eItemType::FOOD)
		add_string_to_buf("Equip: Not in combat");
	else {
		
		// unequip
		if(univ.party[pc_num].equip[item_num]) {
			if(univ.party[pc_num].equip[item_num] && univ.party[pc_num].items[item_num].cursed)
				add_string_to_buf("Equip: Item is cursed.           ");
			else {
				univ.party[pc_num].equip[item_num] = false;
				add_string_to_buf("Equip: Unequipped");
				if(univ.party[pc_num].weap_poisoned == item_num && univ.party[pc_num].status[eStatus::POISONED_WEAPON] > 0) {
					add_string_to_buf("  Poison lost.           ");
					univ.party[pc_num].status[eStatus::POISONED_WEAPON] = 0;
				}
			}
		}
		
		else {  // equip
			if(!equippable.count(univ.party[pc_num].items[item_num].variety))
				add_string_to_buf("Equip: Can't equip this item.");
			else {
				for(i = 0; i < 24; i++)
					if(univ.party[pc_num].equip[i]) {
						if(univ.party[pc_num].items[i].variety == univ.party[pc_num].items[item_num].variety)
							num_equipped_of_this_type++;
						num_hands_occupied = num_hands_occupied + num_hands_to_use.count(univ.party[pc_num].items[i].variety);
					}
				
				
				equip_item_type = excluding_types[univ.party[pc_num].items[item_num].variety];
				// Now if missile is already equipped, no more missiles
				if(equip_item_type > 0) {
					for(i = 0; i < 24; i++)
						if((univ.party[pc_num].equip[i]) && (excluding_types[univ.party[pc_num].items[i].variety] == equip_item_type)) {
							add_string_to_buf("Equip: You have something of");
							add_string_to_buf("  this type equipped.");
							return;
						}
				}
				
				size_t hands_free = 2 - num_hands_occupied;
				if(is_combat() && univ.party[pc_num].items[item_num].variety == eItemType::ARMOR)
					add_string_to_buf("Equip: Not armor in combat");
				else if(hands_free < num_hands_to_use.count(univ.party[pc_num].items[item_num].variety))
					add_string_to_buf("Equip: Not enough free hands");
				else if(equippable.count(univ.party[pc_num].items[item_num].variety) <= num_equipped_of_this_type)
					add_string_to_buf("Equip: Can't equip another");
				else {
					univ.party[pc_num].equip[item_num] = true;
					add_string_to_buf("Equip: OK");
				}
			}
			
		}
	}
	if(stat_window == pc_num)
		put_item_screen(stat_window,1);
}


void drop_item(short pc_num,short item_num,location where_drop) {
	std::string choice;
	short how_many = 0;
	cItem item_store;
	bool take_given_item = true;
	location loc;
	
	item_store = univ.party[pc_num].items[item_num];
	
	if(univ.party[pc_num].equip[item_num] && univ.party[pc_num].items[item_num].cursed)
		add_string_to_buf("Drop: Item is cursed.           ");
	else switch(overall_mode) {
		case MODE_OUTDOORS:
			choice = cChoiceDlog("drop-item-confirm",{"okay","cancel"}).show();
			if(choice == "cancel")
				return;
			add_string_to_buf("Drop: OK");
			if((item_store.type_flag > 0) && (item_store.charges > 1)) {
				how_many = get_num_of_items(item_store.charges);
				if(how_many == item_store.charges)
					univ.party[pc_num].take_item(item_num);
				else univ.party[pc_num].items[item_num].charges -= how_many;
			}
			else univ.party[pc_num].take_item(item_num);
			break;
			
		case MODE_DROP_TOWN: case MODE_DROP_COMBAT:
			loc = where_drop;
			if((item_store.type_flag > 0) && (item_store.charges > 1)) {
				how_many = get_num_of_items(item_store.charges);
				if(how_many <= 0)
					return;
				if(how_many < item_store.charges)
					take_given_item = false;
				item_store.charges = how_many;
			}
			if(is_container(loc))
				item_store.contained = true;
			if(!place_item(item_store,loc,false)) {
				add_string_to_buf("Drop: Too many items on ground");
				item_store.contained = false;
			}
			else {
				if(item_store.contained)
					add_string_to_buf("Drop: Item put away");
				else add_string_to_buf("Drop: OK");
				univ.party[pc_num].items[item_num].charges -= how_many;
				if(take_given_item)
					univ.party[pc_num].take_item(item_num);
			}
			break;
		default: //should never be reached
			break;
	}
}

bool place_item(cItem item,location where,bool forced,bool contained) {
	short i;
	
	if(contained && !is_container(where))
		contained = false;
	
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].variety == eItemType::NO_ITEM) {
			univ.town.items[i] = item;
			univ.town.items[i].item_loc = where;
			univ.town.items[i].contained = contained;
			reset_item_max();
			return true;
		}
	if(!forced)
		return false;
	destroy_an_item();
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].variety == eItemType::NO_ITEM) {
			univ.town.items[i] = item;
			univ.town.items[i].item_loc = where;
			univ.town.items[i].contained = contained;
			reset_item_max();
			return true;
		}
	
	return true;
}

void destroy_an_item() {
	short i;
	ASB("Too many items. Some item destroyed.");
//	for(i = 0; i < NUM_TOWN_ITEMS; i++)
//		if(univ.town.items[i].type_flag == 15) {
//			univ.town.items[i].variety = eItemType::NO_ITEM;
//			return;
//		}
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].value < 3) {
			univ.town.items[i].variety = eItemType::NO_ITEM;
			return;
		}
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].value < 30) {
			univ.town.items[i].variety = eItemType::NO_ITEM;
			return;
		}
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(!univ.town.items[i].magic) {
			univ.town.items[i].variety = eItemType::NO_ITEM;
			return;
		}
	i = get_ran(1,0,NUM_TOWN_ITEMS);
	univ.town.items[i].variety = eItemType::NO_ITEM;
	
}

void give_thing(short pc_num, short item_num) {
	short who_to,how_many = 0;
	cItem item_store;
	bool take_given_item = true;
	
	if(univ.party[pc_num].equip[item_num] && univ.party[pc_num].items[item_num].cursed)
		add_string_to_buf("Give: Item is cursed.           ");
	else {
		item_store = univ.party[pc_num].items[item_num];
		who_to = char_select_pc(3,"Give item to who?");
		if((overall_mode == MODE_COMBAT) && !adjacent(univ.party[pc_num].combat_pos,univ.party[who_to].combat_pos)) {
			add_string_to_buf("Give: Must be adjacent.");
			who_to = 6;
		}
		
		if((who_to < 6) && (who_to != pc_num)
			&& ((overall_mode != MODE_COMBAT) || (adjacent(univ.party[pc_num].combat_pos,univ.party[who_to].combat_pos)))) {
			if((item_store.type_flag > 0) && (item_store.charges > 1)) {
				how_many = get_num_of_items(item_store.charges);
				if(how_many == 0)
					return;
				if(how_many < item_store.charges)
					take_given_item = false;
				univ.party[pc_num].items[item_num].charges -= how_many;
				item_store.charges = how_many;
			}
			if(univ.party[who_to].give_item(item_store,0)) {
				if(take_given_item)
					univ.party[pc_num].take_item(item_num);
			}
			else {
				if(univ.party[who_to].has_space() == 24)
					ASB("Can't give: PC has max. # of items.");
				else ASB("Can't give: PC carrying too much.");
				if(how_many > 0)
					univ.party[pc_num].items[item_num].charges += how_many;
			}
		}
	}
}

// Procedure only ready for town and outdoor
short dist_from_party(location where) {
	short store = 1000, i;
	
	if((overall_mode >= MODE_COMBAT) && (overall_mode < MODE_TALKING)) {
		for(i = 0; i < 6; i++)
			if(univ.party[i].main_status == eMainStatus::ALIVE)
				store = min(store,dist(univ.party[i].combat_pos,where));
	}
	else store = dist(univ.town.p_loc,where);
	
	return store;
}

// TODO: I have no idea what is going on here, other than that it seems to have something to do items being picked up in town
void set_item_flag(cItem* item) {
	if((item->is_special > 0) && (item->is_special < 65)) {
		item->is_special--;
		univ.party.item_taken[univ.town.num][item->is_special / 8] =
			univ.party.item_taken[univ.town.num][item->is_special / 8] | s_pow(2,item->is_special % 8);
		item->is_special = 0;
	}
}

//short pc_num; // if 6, any
short get_item(location place,short pc_num,bool check_container) {
	short i,taken = 0;
	bool item_near = false;
	short mass_get = 1;
	
	for(i = 0; i < univ.town->max_monst(); i++)
		if((univ.town.monst[i].active > 0) && (univ.town.monst[i].attitude == 1)
			&& (can_see_light(place,univ.town.monst[i].cur_loc,sight_obscurity) < 5))
			mass_get = 0;
	
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].variety != eItemType::NO_ITEM)
			if(((adjacent(place,univ.town.items[i].item_loc)) ||
				 (mass_get == 1 && !check_container &&
				  ((dist(place,univ.town.items[i].item_loc) <= 4) || ((is_combat()) && (which_combat_type == 0)))
				  && (can_see_light(place,univ.town.items[i].item_loc,sight_obscurity) < 5)))
				&& ((!univ.town.items[i].contained) || (check_container))) {
				taken = 1;
				
				if(univ.town.items[i].value < 2)
					univ.town.items[i].ident = true;
				item_near = true;
			}
	if(item_near)
		if(display_item(place,pc_num,mass_get,check_container)) { // if true, there was a theft
			for(i = 0; i < univ.town->max_monst(); i++)
				if((univ.town.monst[i].active > 0) && (univ.town.monst[i].attitude % 2 != 1)
					&& (can_see_light(place,univ.town.monst[i].cur_loc,sight_obscurity) < 5)) {
					make_town_hostile();
					i = univ.town->max_monst();
					add_string_to_buf("Your crime was seen!");
				}
		}
	
	if(pc_num != 10) {
		if(taken == 0)
			add_string_to_buf("Get: nothing here");
		else add_string_to_buf("Get: OK");
	}
	
	reset_item_max();
	
	return taken;
	
}

void make_town_hostile() {
	set_town_attitude(0, -1, 1);
	return;
}

// Set Attitude node adapted from *i, meant to replace make_town_hostile node
// att is any valid monster attitude (so, 0..3)
void set_town_attitude(short lo,short hi,short att) {
	short i,num;
	short a[3] = {}; // Dummy values to pass to run_special.
	
	if(which_combat_type == 0)
		return;
	give_help(53,0);
	univ.town.monst.friendly = 1;
	
	// Nice smart indexing, like Python :D
	if(lo <= -univ.town->max_monst())
		lo = 0;
	if(lo < 0)
		lo = univ.town->max_monst() + lo;
	if(hi <= -univ.town->max_monst())
		hi = 0;
	if(hi < 0)
		hi = univ.town->max_monst() + hi;
	if(hi < lo)
		std::swap(lo, hi);
	
	for(i = lo; i <= hi; i++) {
		if((univ.town.monst[i].active > 0) && (univ.town.monst[i].summoned == 0)){
			univ.town.monst[i].attitude = att;
			num = univ.town.monst[i].number;
			// If made hostile, make mobile
			if(att == 1 || att == 3) {
				
				univ.town.monst[i].mobility = 1;
				// If a "guard", give a power boost
				if(univ.scenario.scen_monsters[num].guard) {
					univ.town.monst[i].active = 2;
					univ.town.monst[i].health *= 3;
					univ.town.monst[i].status[eStatus::HASTE_SLOW] = 8;
					univ.town.monst[i].status[eStatus::BLESS_CURSE] = 8;
				}
				
			}
		}
	}
	
	// In some towns, doin' this'll getcha' killed.
	// (Or something else! Killing the party would be the responsibility of whatever special node is called.)
	if((att == 1 || att == 3) && univ.town->spec_on_hostile >= 0)
		run_special(eSpecCtx::TOWN_HOSTILE, 2, univ.town->spec_on_hostile, univ.party.p_loc, &a[0], &a[1], &a[2]);
}

// TODO: Set town status to "dead"? Could reuse above with magic att (eg -1), or write new function.


static void put_item_graphics(cDialog& me, size_t& first_item_shown, short& current_getting_pc, const std::vector<cItem*>& item_array) {
	short i;
	cItem item;
	
	// First make sure all arrays for who can get stuff are in order.
	if(current_getting_pc < 6 && (univ.party[current_getting_pc].main_status != eMainStatus::ALIVE
			|| univ.party[current_getting_pc].has_space() == 24)) {
	 	current_getting_pc = 6;
	 	
	}
	
	for(i = 0; i < 6; i++) {
		std::ostringstream sout;
		sout << "pc" << i + 1;
		std::string id = sout.str();
		if(univ.party[i].main_status == eMainStatus::ALIVE && univ.party[i].has_space() < 24
		   && ((!is_combat()) || (current_pc == i))) {
			if(current_getting_pc == 6)
				current_getting_pc = i;
			me[id].show();
		} else {
			me[id].hide();
			sout << "-g";
			me[sout.str()].hide();
		}
		if(current_getting_pc == i)
			me.addLabelFor(id, "*   ", LABEL_LEFT, 7, true);
		else me.addLabelFor(id,"    ", LABEL_LEFT, 7, true);
	}
	
	// darken arrows, as appropriate
	if(first_item_shown == 0)
		me["up"].hide();
	else me["up"].show();
	if(first_item_shown > item_array.size() - 7 ||
	   item_array.size() <= 8)
		me["down"].hide();
	else me["down"].show();
	
	for(i = 0; i < 8; i++) {
		std::ostringstream sout;
		sout << "item" << i + 1;
		std::string pict = sout.str() + "-g", name = sout.str() + "-name";
		std::string detail = sout.str() + "-detail", weight = sout.str() + "-weight";
		
		try {
			if(item_array.at(i + first_item_shown)->variety == eItemType::NO_ITEM)
				throw std::out_of_range("");
			// display an item in window
			me[pict].show();
			item = *item_array[i + first_item_shown];
			me[name].setText(item.ident ? item.full_name : item.name);
			// TODO: Party sheet items
			cPict& pic = dynamic_cast<cPict&>(me[pict]);
			if(item.graphic_num >= 1000)
				pic.setPict(item.graphic_num - 1000, PIC_CUSTOM_ITEM);
			else pic.setPict(item.graphic_num, PIC_ITEM);
			// TODO: This code is currently kept here for reference to the changed numbers. It can be removed after verifying it works correctly.
#if 0
			if(item.graphic_num >= 1000) // was 150
				csp(987,20 + i * 4,/*3000 + 2000 + */item.graphic_num - 1000,PICT_CUSTOM + PICT_ITEM);
			else csp(987,20 + i * 4,/*4800 + */item.graphic_num,PICT_ITEM);
#endif
			me[detail].setText(get_item_interesting_string(item));
			me[weight].setText("Weight: " + std::to_string(item.item_weight()));
		} catch(std::out_of_range) { // erase the spot
			me[pict].hide();
			me[name].setText("");
			me[detail].setText("");
			me[weight].setText("");
		}
	}
	
	if(current_getting_pc < 6) {
		std::ostringstream sout;
		sout << univ.party[current_getting_pc].name << " is carrying ";
		sout << univ.party[current_getting_pc].cur_weight() << " out of " << univ.party[current_getting_pc].max_weight() << '.';
		me["prompt"].setText(sout.str());
	}
	
	for(i = 0; i < 6; i++)
		if(univ.party[i].main_status == eMainStatus::ALIVE) {
			std::ostringstream sout;
			sout << "pc" << i + 1 << "-g";
			dynamic_cast<cPict&>(me[sout.str()]).setPict(univ.party[i].which_graphic);
		}
}


static bool display_item_event_filter(cDialog& me, std::string id, size_t& first_item_shown, short& current_getting_pc, std::vector<cItem*>& item_array, bool allow_overload) {
	cItem item;
	
	if(id == "done") {
		me.toast(true);
	} else if(id == "up") {
		if(first_item_shown > 0) {
			first_item_shown -= 8;
			put_item_graphics(me, first_item_shown, current_getting_pc, item_array);
		}
	} else if(id ==  "down") {
		if(first_item_shown + 8 < item_array.size()) {
			first_item_shown += 8;
			put_item_graphics(me, first_item_shown, current_getting_pc, item_array);
		}
	} else if(id.substr(0,2) == "pc") {
		current_getting_pc = id[2] - '1';
		put_item_graphics(me, first_item_shown, current_getting_pc, item_array);
	} else {
		if(current_getting_pc == 6) return true;
		size_t item_hit;
		item_hit = id[4] - '1';
		item_hit += first_item_shown;
		if(item_hit >= item_array.size()) return true;
		item = *item_array[item_hit];
		if(item.property) {
			if(me.getResult<bool>()) {
				std::string choice = cChoiceDlog("steal-item",{"steal","leave"}).show();
				if(choice == "leave") return true;
				me.setResult(true);
			}
			item.property = false;
		}
		
		if(item.variety == eItemType::GOLD) {
			if(item.item_level > 3000)
				item.item_level = 3000;
			set_item_flag(&item);
			give_gold(item.item_level,false);
			play_sound(39); // formerly force_play_sound
		} else if(item.variety == eItemType::FOOD) {
			give_food(item.item_level,false);
			set_item_flag(&item);
			play_sound(62); // formerly force_play_sound
		} else if(item.variety == eItemType::SPECIAL) {
			univ.party.spec_items[item.item_level] = true;
			set_item_flag(&item);
		} else {
			if(!allow_overload && item.item_weight() > univ.party[current_getting_pc].free_weight()) {
				beep(); // TODO: This is a game event, so it should have a game sound, not a system alert.
				me["prompt"].setText("It's too heavy to carry.");
				give_help(38,0,me);
				return true;
			}
			
			set_item_flag(&item);
			play_sound(0); // formerly force_play_sound
			univ.party[current_getting_pc].give_item(item, false, allow_overload);
		}
		*item_array[item_hit] = cItem();
		item_array.erase(item_array.begin() + item_hit);
		put_item_graphics(me, first_item_shown, current_getting_pc, item_array);
	}
	return true;
}

// TODO: Move this to a more appropriate place
bool pc_gworld_loaded = false;

// Returns true is a theft committed
//pc_num;  // < 6 - this pc only  6 - any pc
//short mode; // 0 - adjacent  1 - all in sight
bool display_item(location from_loc,short /*pc_num*/,short mode, bool check_container) {
//	short item_array[130];
	std::vector<cItem*> item_array;
	short i;
	
	make_cursor_sword();
	
	short current_getting_pc = current_pc;
	
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].variety != eItemType::NO_ITEM) {
			if(((adjacent(from_loc,univ.town.items[i].item_loc)) ||
				 (mode == 1 && !check_container &&
				  ((dist(from_loc,univ.town.items[i].item_loc) <= 4) || ((is_combat()) && (which_combat_type == 0)))
				  && (can_see_light(from_loc,univ.town.items[i].item_loc,sight_obscurity) < 5))) &&
				(univ.town.items[i].contained == check_container) &&
				((!check_container) || (univ.town.items[i].item_loc == from_loc))) {
				item_array.push_back(&univ.town.items[i]);
			}
		}
	
	bool stole_something = false;
	if(check_container)
		stole_something = show_get_items("Looking in container:", item_array, current_getting_pc);
	else if(mode == 0)
		stole_something = show_get_items("Getting all adjacent items:", item_array, current_getting_pc);
	else stole_something = show_get_items("Getting all nearby items:", item_array, current_getting_pc);
	
	put_item_screen(stat_window,0);
	put_pc_screen();
	
	return stole_something;
}

bool show_get_items(std::string titleText, std::vector<cItem*>& itemRefs, short pc_getting, bool overload) {
	using namespace std::placeholders;
	size_t first_item = 0;
	
	if(!pc_gworld_loaded)
		pc_gworld.loadFromImage(*ResMgr::get<ImageRsrc>("pcs"));
	
	cDialog itemDialog("get-items");
	auto handler = std::bind(display_item_event_filter, _1, _2, std::ref(first_item), std::ref(pc_getting), std::ref(itemRefs), overload);
	itemDialog.attachClickHandlers(handler, {"done", "up", "down"});
	itemDialog.attachClickHandlers(handler, {"pc1", "pc2", "pc3", "pc4", "pc5", "pc6"});
	itemDialog.setResult(false);
	cTextMsg& title = dynamic_cast<cTextMsg&>(itemDialog["title"]);
	
	title.setText(titleText);
	
	for(int i = 1; i <= 8; i++) {
		std::ostringstream sout;
		sout << "item" << i << "-key";
		itemDialog[sout.str()].attachKey({false, static_cast<unsigned char>('`' + i), mod_none});
		itemDialog[sout.str()].attachClickHandler(handler);
	}
	put_item_graphics(itemDialog, first_item, pc_getting, itemRefs);
	
	if(univ.party.help_received[36] == 0) {
		give_help(36,37,itemDialog);
	}
	
	itemDialog.run();
	
	return itemDialog.getResult<bool>();
	
	
}

short custom_choice_dialog(std::array<std::string, 6>& strs,short pic_num,ePicType pic_type,std::array<short, 3>& buttons) {
	make_cursor_sword();
	
	std::vector<std::string> vec;
	std::copy(strs.begin(), strs.end(), std::inserter(vec, vec.begin()));
	cThreeChoice customDialog(vec, buttons, pic_num, pic_type);
	std::string item_hit = customDialog.show();
	
	for(int i = 0; i < 3; i++) {
		auto& btn = basic_buttons[available_btns[buttons[i]]];
		if(item_hit == btn.label)
			return i + 1;
	}
	return -1;
}

void custom_pic_dialog(std::string title, pic_num_t bigpic) {
	cDialog pic_dlg("show-map");
	cControl& okay = pic_dlg["okay"];
	cControl& text = pic_dlg["title"];
	okay.attachClickHandler(std::bind(&cDialog::toast, &pic_dlg, false));
	text.setText(title);
	cPict& map = dynamic_cast<cPict&>(pic_dlg["map"]);
	// We don't provide a way to use non-custom full sheets - why would you want to show standard help graphics?
	map.setPict(bigpic, PIC_CUSTOM_FULL);
	// Now we need to adjust the size to ensure that everything fits correctly.
	map.recalcRect();
	rectangle mapBounds = map.getBounds();
	rectangle btnBounds = okay.getBounds();
	rectangle txtBounds = text.getBounds();
	btnBounds.offset(-btnBounds.left, -btnBounds.top);
	btnBounds.offset(mapBounds.right - btnBounds.width(), mapBounds.bottom + 10);
	okay.setBounds(btnBounds);
	txtBounds.right = mapBounds.right;
	text.setBounds(txtBounds);
	pic_dlg.recalcRect();
	pic_dlg.run();
}

void story_dialog(std::string title, str_num_t first, str_num_t last, int which_str_type, pic_num_t pic, ePicType pt) {
	cDialog story_dlg("many-str");
	dynamic_cast<cPict&>(story_dlg["pict"]).setPict(pic, pt);
	str_num_t cur = first;
	story_dlg.attachClickHandlers([&cur,first,last,which_str_type](cDialog& me, std::string clicked, eKeyMod) -> bool {
		if(clicked == "left") {
			if(cur > first) cur--;
		} else if(clicked == "done" || cur == last) {
			me.toast(false);
			return true;
		} else if(clicked == "right") {
			cur++;
		}
		if(which_str_type == 0)
			me["str"].setText(univ.scenario.spec_strs[cur]);
		else if(which_str_type == 1)
			me["str"].setText(univ.out->spec_strs[cur]);
		else if(which_str_type == 2)
			me["str"].setText(univ.town->spec_strs[cur]);
		return true;
	}, {"left", "right", "done"});
	story_dlg["left"].triggerClickHandler(story_dlg, "left", eKeyMod());
	story_dlg["title"].setText(title);
	story_dlg.run();
}

static bool get_num_of_items_event_filter(cDialog& me, std::string, eKeyMod) {
	if(me.toast(true))
		me.setResult<int>(me["number"].getTextAsNum());
	return true;
}

//town_num; // Will be 0 - 200 for town, 200 - 290 for outdoors
//short sign_type; // terrain type
short get_num_of_items(short max_num) {
	make_cursor_sword();
	
	cDialog numPanel("get-num");
	numPanel.attachClickHandlers(get_num_of_items_event_filter, {"okay"});
	
	numPanel["prompt"].setText("How many? (0-" + std::to_string(max_num) + ") ");
	numPanel["number"].setTextToNum(max_num);
	numPanel.run();
	
	return minmax(0,max_num,numPanel.getResult<int>());
}

void init_mini_map() {
	// TODO: I'm not sure if the bounds in the DLOG resource included the titlebar height; perhaps the actual height should be a little less
	mini_map.create(sf::VideoMode(296,277), "Map", sf::Style::Titlebar | sf::Style::Close);
	mini_map.setPosition(sf::Vector2i(52,62));
	mini_map.setVisible(false);
	setWindowFloating(mini_map, true);
	makeFrontWindow(mainPtr);
}

void make_cursor_watch() {
	set_cursor(watch_curs);
}

void place_glands(location where,mon_num_t m_type) {
	cItem store_i;
	cMonster monst;
	
	if(m_type >= 10000)
		monst = univ.party.summons[m_type - 10000];
	else monst = univ.scenario.scen_monsters[m_type];
	
	if((monst.corpse_item >= 0) && (monst.corpse_item < 400) && (get_ran(1,1,100) < monst.corpse_item_chance)) {
		store_i = get_stored_item(monst.corpse_item);
		place_item(store_i,where,false);
	}
}

short party_total_level() {
	short i,j = 0;
	
	for(i = 0; i < 6; i++)
		if(univ.party[i].main_status == eMainStatus::ALIVE)
			j += univ.party[i].level;
	return j;
}

void reset_item_max() {
	short i;
	
	for(i = 0; i < NUM_TOWN_ITEMS; i++)
		if(univ.town.items[i].variety != eItemType::NO_ITEM)
			item_max = i + 1;
}

short item_val(cItem item) {
	if(item.charges == 0)
		return item.value;
	return item.charges * item.value;
}

//// rewrite
//short mode;  // 0 - normal, 1 - force
void place_treasure(location where,short level,short loot,short mode) {
	
	cItem new_item;
	short amt,r1,i,j;
	// Make these static const because they are never changed.
	// Saves them being initialized every time the function is called.
	static const short treas_chart[5][6] = {
		{0,-1,-1,-1,-1,-1},
		{1,-1,-1,-1,-1,-1},
		{2,1,1,-1,-1,-1},
		{3,2,1,1,-1,-1},
		{4,3,2,2,1,1}
	};
	static const short treas_odds[5][6] = {
		{10,0,0,0,0,0},
		{50,0,0,0,0,0},
		{60,50,40,0,0,0},
		{100,90,80,70,0,0},
		{100,80,80,75,75,75}
	};
	static const short id_odds[21] = {
		0,10,15,20,25,30,35,
		39,43,47,51,55,59,63,
		67,71,73,75,77,79,81
	};
	static const short max_mult[5][10] = {
		{0,0,0,0,0,0,0,0,0,1},
		{0,0,1,1,1,1,2,3,5,20},
		{0,0,1,1,2,2,4,6,10,25},
		{5,10,10,10,15,20,40,80,100,100},
		{25,25,50,50,50,100,100,100,100,100}
	};
	static const short min_chart[5][10] = {
		{0,0,0,0,0,0,0,0,0,1},
		{0,0,0,0,0,0,0,0,5,20},
		{0,0,0,0,1,1,5,10,15,40},
		{10,10,15,20,20,30,40,50,75,100},
		{50,100,100,100,100,200,200,200,200,200}
	};
	short max,min;
	
	if(loot == 1)
		amt = get_ran(2,1,7) + 1;
	else amt = loot * (get_ran(1,0,10 + (loot * 6) + (level * 2)) + 5);
	
	if(party_total_level() <= 12)
		amt += 1;
	if((party_total_level() <= 60)	&& (amt > 2))
		amt += 2;
	
	if(amt > 3) {
		new_item = get_stored_item(0);
		new_item.item_level = amt;
		r1 = get_ran(1,1,9);
		if(((loot > 1) && (r1 < 7)) || ((loot == 1) && (r1 < 5)) || (mode == 1)
			|| ((r1 < 6) && (party_total_level() < 30)) || (loot > 2) )
			place_item(new_item,where,false);
	}
	for(j = 0; j < 5; j++) {
		r1 = get_ran(1,1,100);
		if((treas_chart[loot][j] >= 0) && (r1 <= treas_odds[loot][j] + check_party_stat(eSkill::LUCK, 0))) {
			r1 = get_ran(1,0,9);
			min = min_chart[treas_chart[loot][j]][r1];
			r1 = get_ran(1,0,9);
			max = (min + level + (2 * (loot - 1)) + (check_party_stat(eSkill::LUCK, 0) / 3)) * max_mult[treas_chart[loot][j]][r1];
			if(get_ran(1,0,1000) == 500) {
				max = 10000;
				min = 100;
			}
			
			// reality check
			if((loot == 1) && (max > 100) && (get_ran(1,0,8) < 7))
				max = 100;
			if((loot == 2) && (max > 200) && (get_ran(1,0,8) < 6))
				max = 200;
			
			
			new_item = return_treasure(treas_chart[loot][j]);
			if((item_val(new_item) < min) || (item_val(new_item) > max)) {
				new_item = return_treasure(treas_chart[loot][j]);
				if((item_val(new_item) < min) || (item_val(new_item) > max)) {
					new_item = return_treasure(treas_chart[loot][j]);
					if(item_val(new_item) > max)
						new_item.variety = eItemType::NO_ITEM;
				}
			}
			
			// not many magic items
			if(mode == 0) {
				if(new_item.magic && (level < 2) && (get_ran(1,0,5) < 3))
					new_item.variety = eItemType::NO_ITEM;
				if(new_item.magic && (level == 2) && (get_ran(1,0,5) < 2))
					new_item.variety = eItemType::NO_ITEM;
				if(new_item.cursed && (get_ran(1,0,5) < 3))
					new_item.variety = eItemType::NO_ITEM;
			}
			
			// if forced, keep dipping until a treasure comes up
			if((mode == 1)	&& (max >= 20)) {
				do new_item = return_treasure(treas_chart[loot][j]);
				while(new_item.variety == eItemType::NO_ITEM || item_val(new_item) > max);
			}
			
			// Not many cursed items
			if(new_item.cursed && (get_ran(1,0,2) == 1))
				new_item.variety = eItemType::NO_ITEM;
			
			if(new_item.variety != eItemType::NO_ITEM) {
				for(i = 0; i < 6; i++)
					if((univ.party[i].main_status == eMainStatus::ALIVE)
					   && get_ran(1,1,100) < id_odds[univ.party[i].skill(eSkill::ITEM_LORE)])
						new_item.ident = true;
				place_item(new_item,where,false);
			}
		}
	}
}

cItem return_treasure(short loot) {
	cItem treas;
	static const short which_treas_chart[48] = {
		1,1,1,1,1,2,2,2,2,2,
		3,3,3,3,3,2,2,2,4,4,
		4,4,5,5,5,6,6,6,7,7,
		7,8,8,9,9,10,11,12,12,13,
		13,14, 9,10,11,9,10,11
	};
	short r1;
	
	treas.variety = eItemType::NO_ITEM;
	r1 = get_ran(1,0,41);
	if(loot >= 3)
		r1 += 3;
	if(loot == 4)
		r1 += 3;
	switch(which_treas_chart[r1]) {
		case 1: treas = get_food(); break;
		case 2: treas = get_weapon(loot);	break;
		case 3: treas = get_armor(loot); break;
		case 4: treas = get_shield(loot); break;
		case 5: treas = get_helm(loot); break;
		case 6: treas = get_missile(loot); break;
		case 7: treas = get_potion(loot); break;
		case 8: treas = get_scroll(loot); break;
		case 9: treas = get_wand(loot); break;
		case 10: treas = get_ring(loot); break;
		case 11: treas = get_necklace(loot); break;
		case 12: treas = get_poison(loot); break;
		case 13: treas = get_gloves(loot); break;
		case 14: treas = get_boots(loot); break;
	}
	if(treas.variety == eItemType::NO_ITEM)
		treas.value = 0;
	return treas;
	
}

void refresh_store_items() {
	short i,j;
	short loot_index[10] = {1,1,1,1,2,2,2,3,3,4};
	
	for(i = 0; i < 5; i++)
		for(j = 0; j < 10; j++) {
			univ.party.magic_store_items[i][j] = return_treasure(loot_index[j]);
			if(univ.party.magic_store_items[i][j].variety == eItemType::GOLD ||
			   univ.party.magic_store_items[i][j].variety == eItemType::SPECIAL ||
			   univ.party.magic_store_items[i][j].variety == eItemType::FOOD)
				univ.party.magic_store_items[i][j] = cItem();
			univ.party.magic_store_items[i][j].ident = true;
		}
	
}


static bool get_text_response_event_filter(cDialog& me, std::string, eKeyMod) {
	me.toast(true);
	me.setResult(me["response"].getText());
	return true;
}

std::string get_text_response(std::string prompt, pic_num_t pic) {
	make_cursor_sword();
	
	cDialog strPanel("get-response");
	strPanel.attachClickHandlers(get_text_response_event_filter, {"okay"});
	if(!prompt.empty()) {
		dynamic_cast<cPict&>(strPanel["pic"]).setPict(pic);
		strPanel["prompt"].setText(prompt);
	}
	
	strPanel.run();
	// Note: Originally it only changed the first 15 characters.
	std::string result = strPanel.getResult<std::string>();
	std::transform(result.begin(), result.end(), result.begin(), tolower);
	return result;
}

short get_num_response(short min, short max, std::string prompt) {
	std::ostringstream sout(prompt);
	
	make_cursor_sword();
	
	cDialog numPanel("get-num");
	numPanel.attachClickHandlers(get_num_of_items_event_filter, {"okay"});
	
	sout << " (" << min << '-' << max << ')';
	numPanel["prompt"].setText(sout.str());
	numPanel["number"].setTextToNum(0);
	if(min < max)
		numPanel["number"].attachFocusHandler([min,max](cDialog& me,std::string,bool losing) -> bool {
			if(!losing) return true;
			int val = me["number"].getTextAsNum();
			if(val < min || val > max) {
				giveError("Number out of range!");
				return false;
			}
			return true;
		});
	numPanel.run();
	
	return numPanel.getResult<int>();
}

static bool select_pc_event_filter (cDialog& me, std::string item_hit, eKeyMod) {
	me.toast(true);
	if(item_hit != "cancel") {
		short which_pc = item_hit[item_hit.length() - 1] - '1';
		me.setResult<short>(which_pc);
	} else me.setResult<short>(6);
	return true;
}

// mode determines which PCs can be picked
// 0 - only living pcs, 1 - any pc, 2 - only dead pcs, 3 - only living pcs with inventory space
short char_select_pc(short mode,const char *title) {
	short item_hit,i;
	
	make_cursor_sword();
	
	cDialog selectPc("select-pc");
	selectPc.attachClickHandlers(select_pc_event_filter, {"cancel", "pick1", "pick2", "pick3", "pick4", "pick5", "pick6"});
	
	selectPc["title"].setText(title);
	
	for(i = 0; i < 6; i++) {
		std::string n = boost::lexical_cast<std::string>(i + 1);
		bool can_pick = true;
		if(univ.party[i].main_status == eMainStatus::ABSENT || univ.party[i].main_status == eMainStatus::FLED)
			can_pick = false;
		else switch(mode) {
			case 3:
				if(univ.party[i].has_space() == 24)
					can_pick = false;
			case 0:
				if(univ.party[i].main_status != eMainStatus::ALIVE)
					can_pick = false;
				break;
			case 2:
				if(univ.party[i].main_status == eMainStatus::ALIVE)
					can_pick = false;
				break;
		}
		if(!can_pick) {
			selectPc["pick" + n].hide();
			selectPc["pc" + n].hide();
		} else {
			selectPc["pc" + n].setText(univ.party[i].name);
		}
	}
	
	selectPc.run();
	item_hit = selectPc.getResult<short>();
	
	return item_hit;
}

short select_pc(short mode) {
	return char_select_pc(mode,"Select a character:");
}
