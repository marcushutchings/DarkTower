/*
 * Dark Tower
 * Copyright (C) 2018 Marcus Hutchings
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include <SPI.h>
#include <Gamebuino.h>

#define EVENT_MEMORY 2
#define DESCRIPTION_SIZE 250

#define MAX_OBJECTS_PER_EVENT 10
#define MAX_OBJECTS_ON_PLAYER MAX_OBJECTS_PER_EVENT

#define OBJECT_MENU_LENGTH (MAX_OBJECTS_ON_PLAYER)
#define OBJECT_MENU_ITEM_LENGTH 13

typedef unsigned char action_id;

#define ACTION_ID_NONE 0
#define ACTION_ID_LOOK 0
#define ACTION_ID_TAKE 1
#define ACTION_ID_USE 2
#define ACTION_ID_ITEM 3
#define ACTION_ID_CONTINUE 0xff


typedef unsigned char event_tag_id;

#define EVENT_TAG_ID_CRYPT_MONSTER_DEAD			(1 << 0)
#define EVENT_TAG_ID_SECRET_PASSAGE_OPENED		(1 << 1)
#define EVENT_TAG_ID_RELEASED_SWORD				(1 << 2)
#define EVENT_TAG_ID_UNCOVERED_KEY_MACHINE		(1 << 3)
#define EVENT_TAG_ID_ROPE_TIED_AROUND_WELL		(1 << 4)
#define EVENT_TAG_ID_MASTER_ROOM_DOOR_OPENED	(1 << 6)

#define EVENT_TAG_ID_ALL		 				(0xff)

typedef unsigned char game_state_id;

#define GAME_STATE_ID_TITLE 0
#define GAME_STATE_ID_INIT 1
#define GAME_STATE_ID_PLAY 2
#define GAME_STATE_ID_FINISH_PLAY 3
#define GAME_STATE_ID_PLAYER_DIED 4

typedef unsigned char game_presenter_state_id;

#define GAME_PRESENTER_STATE_ID_AWAIT_ACTION 1
#define GAME_PRESENTER_STATE_ID_AWAIT_OBJECT 2

Gamebuino gb;

extern const byte font5x7[];
extern const byte font3x5[];

#define CALL_REF(the_class, the_function) (the_class.*the_class.the_function)

typedef void (*basic_function)();
typedef void (*load_string_function)(char *string_to_init);

typedef unsigned int player_item_id;

#define PLAYER_ITEM_ID_MASTER_KEY		(1 << 0)
#define PLAYER_ITEM_ID_OIL_LAMP			(1 << 1)
#define PLAYER_ITEM_ID_SWORD			(1 << 2)
#define PLAYER_ITEM_ID_BROKEN_KEY		(1 << 3)
#define PLAYER_ITEM_ID_BLANK_KEY		(1 << 4)
#define PLAYER_ITEM_ID_MOD_CHEST_KEY	(1 << 5)
#define PLAYER_ITEM_ID_SHEET			(1 << 6)
#define PLAYER_ITEM_ID_COPIED_KEY		(1 << 7)
#define PLAYER_ITEM_ID_CHEST_KEY		(1 << 8)
#define PLAYER_ITEM_ID_ROPE				(1 << 9)

#define PLAYER_ITEM_ID_ALL 				(0xffff)

const char player_item_name_master_key[] PROGMEM = "Crystal";
const char player_item_name_lamp[] PROGMEM = "Lamp";
const char player_item_name_broad_sword[] PROGMEM = "Silver Sword";
const char player_item_name_broken_key[] PROGMEM = "Broken Key";
const char player_item_name_blank_key[] PROGMEM = "Blank Key";
const char player_item_name_modified_chest_key[] PROGMEM = "Modified Key";
const char player_item_name_sheet[] PROGMEM = "Curtain";
const char player_item_name_copied_key[] PROGMEM = "Copied Key";
const char player_item_name_chest_key[] PROGMEM = "Copper Key";
const char player_item_name_rope[] PROGMEM = "Rope";

const char* const player_item_name_full_list[] PROGMEM =
	{ player_item_name_master_key
	, player_item_name_lamp
	, player_item_name_broad_sword
	, player_item_name_broken_key
	, player_item_name_blank_key
	, player_item_name_modified_chest_key
	, player_item_name_sheet
	, player_item_name_copied_key
	, player_item_name_chest_key
	, player_item_name_rope
	};

class player_type
{
public:
	player_type()
	: items_carried( 0 )
	, achievements( 0 )
	{ }

	void add_achievement(event_tag_id new_achievement){
		achievements |= new_achievement;
	}

	void remove_achievement(event_tag_id lost_achievement){
		achievements &= ~lost_achievement;
	}

	boolean has_achievement(event_tag_id check_achievement){
		return (achievements & check_achievement);
	}

	void add_item(player_item_id item){
		items_carried |= item;
	}

	void remove_item(player_item_id item){
		items_carried &= ~item;
	}

	boolean has_item(player_item_id item){
		return (items_carried & item) == item;
	}

	player_item_id get_item_by_index(uint8_t item_index){
		player_item_id result = 0;
		uint8_t count = 0;

		for (player_item_id id = 1; id < PLAYER_ITEM_ID_ALL; id <<= 1){
			if (has_item(id)){
				if (count == item_index){
					result = id;
					break;
				}
				count++;
			}
		}
		return result;
	}

	uint8_t load_item_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const char* available_item_list[MAX_OBJECTS_ON_PLAYER];
		char *cur_buf_pos = menu_buffer;
		uint8_t count = 0;

		for (uint8_t i = 0; i < MAX_OBJECTS_ON_PLAYER; i++){
			if ( has_item( 1 << ((uint16_t)i) ) ){
				count++;
				if (count > menu_length)
					break;
				strncpy_P(cur_buf_pos, (char*)pgm_read_word(&(player_item_name_full_list[i])), menu_item_length);
				cur_buf_pos[menu_item_length-1] = '\0';
				cur_buf_pos += menu_item_length;
			}
		}
		return count;
	}

private:
	player_item_id items_carried;
	event_tag_id achievements;
} player;

void load_progmem_string_to_var (const char* progmem_string, char *output_string, uint8_t limit) {
	strncpy_P(output_string, progmem_string, limit);
	output_string[limit-1] = '\0';
}

void copy_string_array_from_src_to_buf(const char** src, uint8_t array_length, char* buf, uint8_t buf_size, uint8_t str_len){
	char *cur_buf_pos = buf;
	char *end_of_buf = buf + buf_size*str_len;
	uint8_t cur_str_len = str_len;

	for (uint8_t i=0; i<array_length; i++){
		if (cur_buf_pos + strlen_P(src[i]) >= end_of_buf)
			cur_str_len = end_of_buf - cur_buf_pos;
		load_progmem_string_to_var(src[i], cur_buf_pos, cur_str_len);
		cur_buf_pos += str_len;
		if (cur_buf_pos >= end_of_buf)
			break;
	}
}

void copy_string_array_from_src_to_buf(const __FlashStringHelper** src, uint8_t array_length, char* buf, uint8_t buf_size, uint8_t str_len){
	copy_string_array_from_src_to_buf((const char **)src, array_length, buf, buf_size, str_len);
}

void append_progmem_string_to_string(const __FlashStringHelper* src, char *dest, uint8_t max_string_length){
	uint16_t used_length = strlen(dest);
	uint16_t remaining_length = max_string_length - used_length;

	strncat_P(dest, (const char*)src, remaining_length);
}

// ------------------------------------------------
// Game Events
// ------------------------------------------------

class event;

typedef void (event::*load_description_type)(char *string_to_init, uint8_t max_string_length);
typedef void (event::*load_object_menu_type)(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length);
typedef event (event::*process_action_on_object_type)(uint8_t selected_action, uint8_t object_selected);
typedef event (event::*process_item_on_object_type)(player_item_id selected_item, uint8_t object_selected);
typedef event (event::*get_prelude_event_type)();
typedef bool (event::*simple_bool_question_type)();

// Do not use virtual methods because the vtable resides in both progmem, and sram.
// Do not add properties to sub-classes otherwise object shearing will result.
class event
{
public:
	event()
	: internal_load_description( &default_load_description )
	, internal_load_object_menu( &default_load_object_menu )
	, internal_process_action_on_object( &default_process_action_on_object )
	, internal_process_item_on_object( &default_process_item_on_object )
	, internal_get_continue_event( &default_get_continue_event )
	, internal_actions_are_allowed( &default_actions_are_allowed )
	, internal_return_to_previous_event( &default_should_return_to_previous_event )
	, description( (const char*)F("Nothing happens.") )
	, allow_actions( false )
	, return_to_previous_event( true )
	, local_event_tags( 0 )
	{ }

	event(const __FlashStringHelper* progmem_description)
	: event()
	{
		description = (const char*)progmem_description;
	}

	void load_description(char *string_to_init, uint8_t max_string_length){
		CALL_REF((*this),internal_load_description)(string_to_init, max_string_length);
	}

	void load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		CALL_REF((*this),internal_load_object_menu)(menu_buffer, menu_length, menu_item_length);
	}

	event process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		return CALL_REF((*this),internal_process_action_on_object)(selected_action, object_selected);
	}

	event process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		return CALL_REF((*this),internal_process_item_on_object)(selected_item, object_selected);
	}

	event get_continue_event(){
		return CALL_REF((*this),internal_get_continue_event)();
	}

	bool actions_are_allowed(){
		return CALL_REF((*this),internal_actions_are_allowed)();
	}

	bool should_return_to_previous_event(){
		return CALL_REF((*this),internal_return_to_previous_event)();
	}

protected:
	const char *description;
	bool allow_actions;
	bool return_to_previous_event;
	uint8_t local_event_tags;

	load_description_type internal_load_description;
	load_object_menu_type internal_load_object_menu;
	process_action_on_object_type internal_process_action_on_object;
	process_item_on_object_type internal_process_item_on_object;
	get_prelude_event_type internal_get_continue_event;
	simple_bool_question_type internal_actions_are_allowed;
	simple_bool_question_type internal_return_to_previous_event;

	void default_load_description(char *string_to_init, uint8_t max_string_length);
	void default_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length) {};
	event default_process_action_on_object(uint8_t selected_action, uint8_t object_selected){ return event(); }
	event default_process_item_on_object(player_item_id selected_item, uint8_t object_selected){ return event(); }
	event default_get_continue_event() { return event(); }
	bool default_actions_are_allowed() { return allow_actions; }
	bool default_should_return_to_previous_event() { return return_to_previous_event; }
};

void event::default_load_description(char *string_to_init, uint8_t max_string_length)
{
	load_progmem_string_to_var(description, string_to_init, max_string_length);
}

struct
{
	game_state_id state = GAME_STATE_ID_TITLE;
} game_state;

const char drink_pink_vial[] PROGMEM = "You take the pink vial and drink it. It tastes foul. A few moments later, you start coughing blood violently and collapse. Everything goes dark.";
const char open_chest_description[] PROGMEM = "The key slots in and turns. The chest unlocks and you open it. The door slams shut behind you. Inside you see ";
const char f0_hall_description[] PROGMEM = "The light from your lamp shows this is a store room with many barrels. There is a well in the middle of the room.";

event create_f0_main_hall_event();
event create_f1_main_hall_event();
event create_f2_main_hall_event();
event create_f3_main_hall_event();

class intro_event : public event {
public:
	intro_event(){
		description = (const char*)F("Stairs lead up to the first floor of the abandoned tower. A tower flowing with what Angels fear; the dark. You ascend hoping to find a way to break the curse of undeath that has come upon you. The tower's doors close behind you. You are trapped!");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
		player.remove_achievement(EVENT_TAG_ID_ALL);
		player.remove_item(PLAYER_ITEM_ID_ALL);

		// temp for testing - 6 bytes needed
		//player.add_item(PLAYER_ITEM_ID_SHEET);
	}

private:
	event real_get_continue_event(){
		return create_f1_main_hall_event();
	}
};

class resurrect_event : public event {
public:
	resurrect_event(){
		description = (const char*)F("You wake up in the entrance hall of the tower. Not sure of what has happened, you find you have lost your items!");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
		player.remove_achievement(EVENT_TAG_ID_ALL);
		player.remove_item(PLAYER_ITEM_ID_ALL);
	}

private:
	event real_get_continue_event(){
		return create_f1_main_hall_event();
	}
};

class return_to_game : public event {
public:
	return_to_game(){
		description = (const char *)F("Well Done! You have won! If you would like to play again, then please continue.");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
	}

protected:
	event real_get_continue_event(){
		return intro_event();
	}
};

class win_event : public event {
public:
	win_event(){
		description = (const char *)F("You leave the tower feeling reborn. The ordeal of the tower may well live with you forever, but now no longer bearing the curse of undeath you can explore and enjoy everything the world has to offer.");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
	}

protected:
	event real_get_continue_event(){
		return return_to_game();
	}
};

class player_dies_event : public event {
public:
	player_dies_event(){
		description = (const char *)F("");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
	}

	player_dies_event(const __FlashStringHelper* progmem_description)
	: player_dies_event()
	{
		description = (const char*)progmem_description;
	}

protected:
	event real_get_continue_event(){
		return resurrect_event();
	}
};

struct drinks_yellow_vial_event : public event
{
	drinks_yellow_vial_event(){
		description = (const char *)F("You take the yellow vial and drink it. It tastes foul. A few moments later, you feel warmth return to your body. You starting breathing again. The potion has cured you of the curse of undeath!");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
	}

protected:
	event real_get_continue_event(){
		return win_event();
	}
};

class f0_in_the_well_event : public event {
public:
	f0_in_the_well_event(){
		description = (const char *)F("The water comes up to your waist, it feels cold.");
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
	}

private:
	static const uint8_t EVENT_NOTICE_KEY = (1<<0);

	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);
		if (should_show_crystal())
			append_progmem_string_to_string(F(" You can see what looks like a crystal in the water."), string_to_init, max_string_length);
	}

	bool should_show_crystal(){
		bool crystal_is_available = !player.has_item(PLAYER_ITEM_ID_MASTER_KEY);
		bool crystal_has_not_been_used = !player.has_achievement(EVENT_TAG_ID_MASTER_ROOM_DOOR_OPENED);

		return (crystal_is_available && crystal_has_not_been_used);
	}

	bool should_show_key(){
		bool key_has_been_noticed = local_event_tags & EVENT_NOTICE_KEY;
		bool key_is_available = !player.has_item(PLAYER_ITEM_ID_BLANK_KEY);
		bool key_is_not_cut = !player.has_item(PLAYER_ITEM_ID_COPIED_KEY);
		bool crystal_is_shown = should_show_crystal();

		return (key_has_been_noticed && key_is_available && crystal_is_shown && key_is_not_cut);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Water")
			, F("Rope")
			, F("Crystal")
			, F("Key")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		object_list_length -= 2;

		if (should_show_crystal()){
			object_list_length++;
			if (should_show_key())
				object_list_length++;
		}

		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The water is clear and stagnant.")
			, F("The rope hangs down from above.")
			, F("The diamond-shaped crystal seems to glow with magical energy.")
			, F("The key is has a no cuttings on its head. It is like a blank key.")
			, F("You notice in the light of the crystal there is a key in water.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			if (object_selected == 0 && should_show_crystal()){
				local_event_tags |= EVENT_NOTICE_KEY;
				object_selected = 4;
			}
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			if (object_selected == 1)
				return create_f0_main_hall_event();
			break;
		case ACTION_ID_TAKE:
			if (object_selected == 2){
				bool key_disapears = should_show_key();
				player.add_item(PLAYER_ITEM_ID_MASTER_KEY);
				if ( key_disapears )
					return event(F("You take the diamond-shaped crystal. The key in the water fades into nothingness."));
				else
					return event(F("You take the diamond-shaped crystal."));
			}
			if (object_selected == 3){
				player.add_item(PLAYER_ITEM_ID_BLANK_KEY);
				return event(F("You take the blank key."));
			}
			break;
		}
		return event();
	}
};

class f0_light_room_event : public event {
public:
	f0_light_room_event(){
		description = f0_hall_description;
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
	}

private:
	static const uint8_t EVENT_NOTICE_ROPE = (1<<0);

	bool should_show_rope(){
		bool rope_has_been_noticed = local_event_tags & EVENT_NOTICE_ROPE;
		bool rope_is_available = !player.has_item(PLAYER_ITEM_ID_ROPE);

		return ( rope_has_been_noticed && rope_is_available );
	}

	bool should_show_crystal(){
		bool crystal_is_available = !player.has_item(PLAYER_ITEM_ID_MASTER_KEY);
		bool crystal_has_not_been_used = !player.has_achievement(EVENT_TAG_ID_MASTER_ROOM_DOOR_OPENED);

		return (crystal_is_available && crystal_has_not_been_used);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Stairs up")
			, F("Barrels")
			, F("Well")
			, F("Rope")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		object_list_length --;

		if (should_show_rope())
			object_list_length++;

		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	const __FlashStringHelper* shimmer_in_water(){
		return F("There seems to be water in the well. Something glitters in the light under the shallow water.");
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The stairs lead up to the light of the entrance room.")
			, F("The barrels contain grain.")
			, F("There seems to be water in the well.")
			, F("The long length of rope is made of hemp and looks strong.")
			, F("One of the barrels contains some rope.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			if (object_selected == 1){
				if ( !player.has_item(PLAYER_ITEM_ID_ROPE) && !player.has_achievement(EVENT_TAG_ID_ROPE_TIED_AROUND_WELL)){
					local_event_tags |= EVENT_NOTICE_ROPE;
					object_selected = 4;
				}
			}
			else if (object_selected == 2){
				if ( should_show_crystal() )
					return event(shimmer_in_water());
			}
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			if (object_selected == 0)
				return create_f1_main_hall_event();
			else if (object_selected == 3 && player.has_achievement(EVENT_TAG_ID_ROPE_TIED_AROUND_WELL))
				return f0_in_the_well_event();
			break;
		case ACTION_ID_TAKE:
			if (object_selected == 3){
				player.add_item(PLAYER_ITEM_ID_ROPE);
				player.remove_achievement(EVENT_TAG_ID_ROPE_TIED_AROUND_WELL);
				return event(F("You gather the rope."));
			}
			break;
		}
		return event();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		if (selected_item == PLAYER_ITEM_ID_ROPE){
			if (object_selected == 2){
				player.add_achievement(EVENT_TAG_ID_ROPE_TIED_AROUND_WELL);
				player.remove_item(PLAYER_ITEM_ID_ROPE);
				return event(F("You tie the rope around the well."));
			}
		}
		else if (selected_item == PLAYER_ITEM_ID_OIL_LAMP){
			if (object_selected == 2){
				if ( should_show_crystal() )
					return event(shimmer_in_water());
				else
					return event(F("The water is still."));
			}
		}
		return event();
	}
};

class f0_monster_dies_event : public event {
public:
	f0_monster_dies_event(){
		description = (const char *)F("The bat flies towards you, fangs ready to bite you. You quickly draw your sword and swing at the creature. The sword cuts the creature and the fell beast screeches and bursts into flames before evaporating into mist.");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
	}

private:
	event real_get_continue_event(){
		return f0_light_room_event();
	}
};

class f0_monster_attacks_event : public event {
public:
	f0_monster_attacks_event(){
		description = f0_hall_description;
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
	}

private:
	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);
		append_progmem_string_to_string(F(" Your light disturbs a large black creature, hanging from the ceiling. The large bat unfolds its wings and attacks you!"), string_to_init, max_string_length);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Stairs up")
			, F("Barrels")
			, F("Well")
			, F("Large Bat")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		switch (selected_action){
		case ACTION_ID_LOOK:
			if (object_selected == 3)
				return event(F("The bat has a five foot wing span and very sharp fangs."));
			break;
		}
		return killed_by_bat_while_distracted();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		if (object_selected == 3){
			if (selected_item == PLAYER_ITEM_ID_SWORD){
				player.add_achievement(EVENT_TAG_ID_CRYPT_MONSTER_DEAD);
				return f0_monster_dies_event();
			}
			return event();
		}
		return killed_by_bat_while_distracted();
	}

	event killed_by_bat_while_distracted(){
		return player_dies_event(F("While distracted the bat grabs you and sinks its fangs deep into your neck. Everything goes dark."));
	}
};

class f0_dark_room_event : public event {
public:
	f0_dark_room_event(){
		description = (const char *)F("You descend several steps, but it quickly gets too dark to proceed further.");
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
	}

private:
	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Stairs up")
			, F("Darkness")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The stairs lead up to the light of the entrance room.")
			, F("This area is too dark to see anything.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			if (object_selected == 0)
				return create_f1_main_hall_event();
		}
		return event();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		switch (selected_item){
		case PLAYER_ITEM_ID_OIL_LAMP:
			if (object_selected == 1){
				if (player.has_achievement(EVENT_TAG_ID_CRYPT_MONSTER_DEAD))
					return f0_light_room_event();
				else
					return f0_monster_attacks_event();
			}
		}
		return event();
	}
};

class f4_open_chest_with_copied_key : public event {
public:
	f4_open_chest_with_copied_key(){
		description = open_chest_description;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
		allow_actions = true;
	}

private:
	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);
		append_progmem_string_to_string(F("two vials, each containing a different coloured liquid."), string_to_init, max_string_length);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Pink Vial")
			, F("Yellow Vial")
			, F("Door")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The small vial contains a pink liquid.")
			, F("The small vial contains a yellow liquid.")
			, F("The door is shut tight. You cannot open it.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
		case ACTION_ID_TAKE:
			switch (object_selected){
			case 0:
				return player_dies_event((const __FlashStringHelper*)drink_pink_vial);
			case 1:
				return drinks_yellow_vial_event();
			}
			return event(look_descriptions[object_selected]);
			break;
		}
		return event();
	}
};

class f4_open_chest_with_modified_key : public event {
public:
	f4_open_chest_with_modified_key(){
		description = open_chest_description;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
		allow_actions = true;
	}

private:
	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);
		append_progmem_string_to_string(F("a vial containing a pink liquid."), string_to_init, max_string_length);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Pink Vial")
			, F("Door")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The small vial contains a pink liquid.")
			, F("The door is shut tight. You cannot open it.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
		case ACTION_ID_TAKE:
			switch (object_selected){
			case 0:
				return player_dies_event((const __FlashStringHelper*)drink_pink_vial);
			}
			return event(look_descriptions[object_selected]);
			break;
		}
		return event();
	}
};

class f4_open_chest_with_chest_key : public event {
public:
	f4_open_chest_with_chest_key(){
		description = open_chest_description;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
		allow_actions = true;
	}

private:
	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);
		append_progmem_string_to_string(F("a vial containing a pink liquid."), string_to_init, max_string_length);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Vial")
			, F("Door")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The small vial contains a pink liquid.")
			, F("The door is shut tight. You cannot open it.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
		case ACTION_ID_TAKE:
			switch (object_selected){
			case 0:
				return player_dies_event((const __FlashStringHelper*)drink_pink_vial);
			}
			return event(look_descriptions[object_selected]);
			break;
		}
		return event();
	}
};

class f4_main_hall_event : public event {
public:
	f4_main_hall_event(){
		description = (const char *)F("Four arrow-slit windows cast a dim light in this room. A chest stands in the middle of the room.");
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
		allow_actions = true;
	}

private:
	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Chest")
			, F("Stairs down")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The chest is sturdy with iron bands and a lock built in. Could this have the cure you are looking for?")
			, F("The stairs lead down to the floor below.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			switch (object_selected){
			case 1:
				return create_f3_main_hall_event();
			}
			break;
		case ACTION_ID_TAKE:
			switch (object_selected){
			case 0:
				return event(F("You try to move the chest, but it won't budge. It is like it is held in place by some force."));
			}
			break;
		}
		return event();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		if (object_selected == 0){
			if (selected_item == PLAYER_ITEM_ID_CHEST_KEY)
				return f4_open_chest_with_chest_key();
			if (selected_item == PLAYER_ITEM_ID_MOD_CHEST_KEY)
				return f4_open_chest_with_modified_key();
			if (selected_item == PLAYER_ITEM_ID_COPIED_KEY)
				return f4_open_chest_with_copied_key();
		}
		return event();
	}
};

class f4_door_unlocked_event : public event {
public:
	f4_door_unlocked_event(){
		description = (const char *)F("You place the crystal in the door. The magic circle and the symbols glow faintly and hum with energy. With a loud grinding sound, the door swings open to reveal the room beyond.");
		internal_get_continue_event = static_cast<get_prelude_event_type>(&real_get_continue_event);
		return_to_previous_event = false;
	}

private:
	event real_get_continue_event(){
		return f4_main_hall_event();
	}
};

class f4_main_hall_locked_event : public event {
public:
	f4_main_hall_locked_event(){
		description = (const char *)F("You ascend the stairs to the next floor. A solid oak door awaits you at the top of the stairs.");
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
	}

private:
	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Stairs Down")
			, F("Door")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The stairs lead down to the hall below.")
			, F("The solid oak door is sturdy and is locked. A magic circle with strange symbols mark the door. In the centre is a diamond shaped hole.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			if (object_selected == 0)
				return create_f3_main_hall_event();
		}
		return event();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		if (selected_item == PLAYER_ITEM_ID_MASTER_KEY)
			if (object_selected == 1){
				player.add_achievement(EVENT_TAG_ID_MASTER_ROOM_DOOR_OPENED);
				return f4_door_unlocked_event();
			}
		return event();
	}
};

class f3_main_hall_event : public event {
public:
	f3_main_hall_event(){
		description = (const char *)F("At opposite ends of the room are stairs; one leads up, one leads down. There is a table of alchemical instruments and broken glass.");
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
	}

private:
	static const uint8_t EVENT_NOTICE_KEY = (1<<0);
	static const uint8_t EVENT_CUT_CHEST_KEY = (1<<1);
	static const uint8_t EVENT_CUT_BLANK_KEY = (1<<2);
	static const uint8_t EVENT_COPY_CHEST_KEY = (1<<3);
	static const uint8_t EVENT_COPY_BROKEN_KEY = (1<<4);
	static const uint8_t EVENT_CUT_KEY_PLACED = (EVENT_CUT_CHEST_KEY|EVENT_CUT_BLANK_KEY);
	static const uint8_t EVENT_COPY_KEY_PLACED = (EVENT_COPY_CHEST_KEY|EVENT_COPY_BROKEN_KEY);

	bool player_can_see_the_key(){
		bool key_is_available = !player.has_item(PLAYER_ITEM_ID_CHEST_KEY);
		bool key_has_been_noticed = (local_event_tags & EVENT_NOTICE_KEY);

		bool key_visible = key_is_available && key_has_been_noticed;

		return key_visible;
	}

	bool key_cutter_is_ready(){
		bool copy_key_placed_in_machine = local_event_tags & EVENT_COPY_KEY_PLACED;
		bool cut_key_placed_in_machine = local_event_tags & EVENT_CUT_KEY_PLACED;

		return (copy_key_placed_in_machine && cut_key_placed_in_machine);
	}

	void make_new_key(){
		uint8_t key_to_cut = local_event_tags & EVENT_CUT_KEY_PLACED;

		switch (key_to_cut){
		case EVENT_CUT_CHEST_KEY:
			player.remove_item(PLAYER_ITEM_ID_CHEST_KEY);
			player.add_item(PLAYER_ITEM_ID_MOD_CHEST_KEY);
			break;
		case EVENT_CUT_BLANK_KEY:
			player.remove_item(PLAYER_ITEM_ID_BLANK_KEY);
			player.add_item(PLAYER_ITEM_ID_COPIED_KEY);
			break;
		}
	}

	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);

		if (player.has_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE))
			append_progmem_string_to_string(F(" Next to it is a key-cutting machine."), string_to_init, max_string_length);
		else
			append_progmem_string_to_string(F(" Next to it a dusty curtain covers something large."), string_to_init, max_string_length);
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Table")
			, F("Curtain")
			, F("Stairs up")
			, F("Stairs down")
			, F("Key")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		object_list_length --;

		if (player.has_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE))
			object_list[1] = F("Key Machine");

		if (player_can_see_the_key())
			object_list_length++;

		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The table is stained with spilled chemicals. The tools and instruments are rusted and broken. In amongst the mess there is a copper key.")
			, F("The elegant red curtain with gold trim completely covers something large and box shaped.")
			, F("The stairs wind up to the next floor.")
			, F("The stairs lead down to the faint light of the hall below.")
			, F("The key is small and made of copper. It has an elegant floral pattern on the handle.")
			, F("The table is stained with spilled chemicals. The tools and instruments are rusted and broken.")
			};

		if (player.has_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE))
			look_descriptions[1] = F("The key cutting machine seems to take first the key you wish to copy and then the cut you wish to cut.");

		switch (selected_action){
		case ACTION_ID_LOOK:
			if (object_selected == 0){
				if (player.has_item(PLAYER_ITEM_ID_CHEST_KEY))
					object_selected = 5;
				local_event_tags |= EVENT_NOTICE_KEY;
			}
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			switch (object_selected){
			case 2:
				if (player.has_achievement(EVENT_TAG_ID_MASTER_ROOM_DOOR_OPENED))
					return f4_main_hall_event();
				else
					return f4_main_hall_locked_event();
			case 3:
				return create_f2_main_hall_event();
			case 1:
				if (key_cutter_is_ready()){
					make_new_key();
					return event(F("You use the machine and get a new key."));
				}
				else{
					if (player.has_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE))
						return event(F("First place a key to copy then one to cut. To reset choices, leave and return to this room."));
				}
			}
			break;
		case ACTION_ID_TAKE:
			switch (object_selected){
			case 1:
				if (!player.has_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE)){
					player.add_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE);
					player.add_item(PLAYER_ITEM_ID_SHEET);
					return event(F("You collect the curtain and uncover what seems to be a key cutting machine."));
				}
				break;
			case 4:
				player.add_item(PLAYER_ITEM_ID_CHEST_KEY);
				return event(F("You pick up the small copper key."));
			}
			break;
		}
		return event();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		switch (object_selected){
		case 1:
			if (player.has_achievement(EVENT_TAG_ID_UNCOVERED_KEY_MACHINE)){
				if (!(local_event_tags & EVENT_COPY_KEY_PLACED)){
					if (selected_item == PLAYER_ITEM_ID_CHEST_KEY)
						return event(F("This key looks okay. It doesn't need copying."));

					if (selected_item == PLAYER_ITEM_ID_BROKEN_KEY){
						local_event_tags |= EVENT_COPY_BROKEN_KEY;
						return event(F("You place the broken key in the machine for copying."));
					}
				}
				else if (!(local_event_tags & EVENT_CUT_KEY_PLACED)){
					if (selected_item == PLAYER_ITEM_ID_CHEST_KEY){
						local_event_tags |= EVENT_CUT_CHEST_KEY;
						return event(F("You place the copper key in the machine for cutting."));
					}
					if (selected_item == PLAYER_ITEM_ID_BLANK_KEY){
						local_event_tags |= EVENT_CUT_BLANK_KEY;
						return event(F("You place the blank key in the machine for cutting."));
					}
				}
			}
		}
		return event();
	}
};

class f2_main_hall_event : public event {
public:
	f2_main_hall_event(){
		description = (const char *)F("A barred window casts a ray of light over a statue of a knightly angel. Stairs continue to lead up as well as down.");
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
		internal_process_item_on_object = static_cast<process_item_on_object_type>(&real_process_item_on_object);
		internal_load_description = static_cast<load_description_type>(&real_load_description);
	}

private:
	void real_load_description(char *string_to_init, uint8_t max_string_length)
	{
		default_load_description(string_to_init, max_string_length);

		if ( sword_is_on_ground() )
			append_progmem_string_to_string(F(" The sword is lying on the ground before the statue."), string_to_init, max_string_length);
	}

	static const uint8_t EVENT_NOTICE_SWORD = (1<<0);
	static const uint8_t EVENT_NOTICE_KEY = (1<<1);

	enum event_object_ids
		{ EVENT_OBJECT_WINDOW
		, EVENT_OBJECT_STATUE
		, EVENT_OBJECT_STAIRS_UP
		, EVENT_OBJECT_STAIRS_DOWN
		, EVENT_OBJECT_SILVER_SWORD
		, EVENT_OBJECT_BROKEN_KEY
		, EVENT_OBJECT_SWORD_ON_FLOOR = EVENT_OBJECT_BROKEN_KEY
		, EVENT_OBJECT_ANGEL_CHANGED
		};

	bool sword_is_on_ground(){
		bool sword_is_released = player.has_achievement(EVENT_TAG_ID_RELEASED_SWORD);
		bool sword_is_available = !player.has_item(PLAYER_ITEM_ID_SWORD);
		return ( sword_is_released && sword_is_available );
	}

	bool sword_should_be_in_object_list(){
		bool sword_is_released = player.has_achievement(EVENT_TAG_ID_RELEASED_SWORD);
		bool sword_is_available = !player.has_item(PLAYER_ITEM_ID_SWORD);
		bool sword_has_been_noticed = (local_event_tags & EVENT_NOTICE_SWORD);

		bool show_sword = sword_is_available && (sword_is_released || sword_has_been_noticed);

		return show_sword;
	}

	bool player_can_access_key(){
		bool sword_is_released = player.has_achievement(EVENT_TAG_ID_RELEASED_SWORD);
		bool sword_is_available = !player.has_item(PLAYER_ITEM_ID_SWORD);
		bool key_is_available = !player.has_item(PLAYER_ITEM_ID_BROKEN_KEY);

		bool key_accesssible = sword_is_released && sword_is_available && key_is_available;

		return key_accesssible;
	}

	bool key_should_be_in_object_list(){
		bool key_is_accessible = player_can_access_key();
		bool key_has_been_noticed = (local_event_tags & EVENT_NOTICE_KEY);

		bool show_key = key_is_accessible && key_has_been_noticed;

		return show_key;
	}

	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Window")
			, F("Angel Statue")
			, F("Stairs up")
			, F("Stairs down")
			, F("Silver Sword")
			, F("Key")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		object_list_length -= 2;

		if ( sword_should_be_in_object_list() )
			object_list_length++;
		if ( key_should_be_in_object_list() )
			object_list_length++;

		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		const __FlashStringHelper* look_descriptions[] =
			{ F("The lonely window has rusted iron bars. A broken rail clings to the wall above the window.")
			, F("The statue is of an angelic knight kneeling before the light of the window. One hand on its breast plate the other holding a silver sword up-side-down.")
			, F("The stairs lead up to the next floor.")
			, F("Stairs lead down to a warm glow.")
			, F("The sword glitters beautifully in the light. It carries a sharp edge.")
			, F("The sword is lying on the stone floor, light reflects off it onto the wall, which becomes translucent revealing a cache. In the cache you see a key.")
			, F("The statue is of an angelic knight kneeling before the window. One hand on its breast plate the other reaching out in despair.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			if (object_selected == EVENT_OBJECT_STATUE){
				if ( !player.has_item(PLAYER_ITEM_ID_SWORD) )
					local_event_tags |= EVENT_NOTICE_SWORD;
				if ( player.has_achievement(EVENT_TAG_ID_RELEASED_SWORD) )
					object_selected = EVENT_OBJECT_ANGEL_CHANGED;
			}
			if (object_selected == EVENT_OBJECT_SILVER_SWORD){
				if ( player_can_access_key() ){
					local_event_tags |= EVENT_NOTICE_KEY;
					object_selected = EVENT_OBJECT_SWORD_ON_FLOOR;
				}
			}
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			switch (object_selected){
			case EVENT_OBJECT_STAIRS_UP:
				return f3_main_hall_event();
			case EVENT_OBJECT_STAIRS_DOWN:
				return create_f1_main_hall_event();
			}
			break;
		case ACTION_ID_TAKE:
			switch (object_selected){
			case EVENT_OBJECT_SILVER_SWORD:
				if ( player.has_achievement(EVENT_TAG_ID_RELEASED_SWORD) ){
					player.add_item(PLAYER_ITEM_ID_SWORD);
					return event(F("The sword is in perfect condition and glistens silver in the light."));
				}else
					return event(F("You are unable to release the sword from the statue's grip."));
				break;
			case EVENT_OBJECT_BROKEN_KEY:
				player.add_item(PLAYER_ITEM_ID_BROKEN_KEY);
				return event(F("The key's handle is broken off and missing."));
			}
			break;
		}
		return event();
	}

	event real_process_item_on_object(player_item_id selected_item, uint8_t object_selected){
		switch (selected_item){
		case PLAYER_ITEM_ID_SHEET:
			if (object_selected == EVENT_OBJECT_WINDOW){
				player.add_achievement(EVENT_TAG_ID_RELEASED_SWORD);
				player.remove_item(PLAYER_ITEM_ID_SHEET);
				return event(F("You cover the window with the curtain. The room is cloaked in darkness. A high pitch scream echoes in the room and then a clang of metal. You drop the curtain to see the sword is now lying on the floor."));
			}
			else if (object_selected == EVENT_OBJECT_STATUE)
				return event(F("The curtain is too small to cover the statue."));
		}
		return event();
	}
};

class f1_main_hall_event : public event {
public:
	f1_main_hall_event(){
		description = (const char *)F("A red carpet leads between the entrance door and stairs that lead up and down. The oil lamps on the wall dimly light the room in dancing shadows.");
		allow_actions = true;
		internal_load_object_menu = static_cast<load_object_menu_type>(&real_load_object_menu);
		internal_process_action_on_object = static_cast<process_action_on_object_type>(&real_process_action_on_object);
	}

private:
	void real_load_object_menu(char *menu_buffer, uint8_t menu_length, uint8_t menu_item_length){
		const __FlashStringHelper* object_list[] =
			{ F("Entrance")
			, F("Stairs up")
			, F("Stairs down")
			, F("Lamp")
			};
		uint8_t object_list_length = sizeof(object_list) / sizeof(typeof(object_list[0]));
		copy_string_array_from_src_to_buf(object_list, object_list_length, menu_buffer, menu_length, menu_item_length);
	}

	event real_process_action_on_object(uint8_t selected_action, uint8_t object_selected){
		// Switch statement approach uses fewer bytes than array lookup, but is more painful to maintain.
		const __FlashStringHelper* look_descriptions[] =
			{ F("The doors are made of old oak. Patterns of trees and falling leaves are carved into the doors.")
			, F("The wooden stairs go up as they wind around the wall, leading to the next floor.")
			, F("The stone stairs hug the wall as they descend into darkness.")
			, F("The lamps are still running; though, they have not been touched for a long time.")
			};

		switch (selected_action){
		case ACTION_ID_LOOK:
			return event(look_descriptions[object_selected]);
		case ACTION_ID_USE:
			switch (object_selected){
			case 0:
				return event(F("The door is shut tight. You cannot open it."));
			case 1:
				return f2_main_hall_event();
			case 2:
				return f0_dark_room_event();
			}
			break;
		case ACTION_ID_TAKE:
			switch (object_selected){
			case 3:
				if ( !(player.has_item(PLAYER_ITEM_ID_OIL_LAMP)) ){
					player.add_item(PLAYER_ITEM_ID_OIL_LAMP);
					return event(F("You take one of the lamps off the wall."));
				}
				else
					return event(F("You already have a lamp!"));
			}
			break;
		}
		return event();
	}
};

event create_f0_main_hall_event(){
	return f0_light_room_event();
}

event create_f1_main_hall_event(){
	return f1_main_hall_event();
}

event create_f2_main_hall_event(){
	return f2_main_hall_event();
}

event create_f3_main_hall_event(){
	return f3_main_hall_event();
}

struct starting_event : public intro_event
{};


// ------------------------------------------------
// Game Engine
// ------------------------------------------------

const char actionStringLook[] = "Look";
const char actionStringTake[] = "Take";
const char actionStringUse[] = "Use";
const char actionStringOpen[] = "Item";
const char actionStringContinue[] = "Continue";

const char* const standard_actions[] =
		{ actionStringLook
		, actionStringTake
		, actionStringUse
		, actionStringOpen
		};

const char* const continue_actions[] = { actionStringContinue };

struct menu_event_handler;

typedef void (menu_event_handler::*menu_selection_handler)(uint8_t action_selected);
typedef void (menu_event_handler::*menu_cancel_handler)();

struct menu_event_handler {
	menu_event_handler()
	: handle_menu_selection( NULL )
	, handle_menu_cancel( NULL )
	{ }

	menu_selection_handler handle_menu_selection;
	menu_cancel_handler handle_menu_cancel;
};

class action_menu_type {
public:
	action_menu_type()
	: actions_allowed( false )
	, selected_action( 0 )
	, width_in_chars( 0 )
	, scroll_delay( 5 )
	, menu_items( 0 )
	, menu_items_count( 0 )
	, registered_menu_handler( NULL )
	, min_line_for_selection( 0 )
	, max_line_for_selection( UINT8_MAX )
	{ }

	void load_menu_from_string_list(const char* const * const string_menu_list, uint8_t menu_list_length){
		menu_items = string_menu_list;
		menu_items_count = menu_list_length;
		width_in_chars = LCDWIDTH / gb.display.fontWidth;
		selected_action = 0;
	}

	void register_menu_handler(menu_event_handler* handler){
		registered_menu_handler = handler;
	}

	void display_portion(uint8_t start_line, uint8_t lines_to_show){
		print_menu( start_line, lines_to_show );
	}

	uint8_t get_display_line_count(){
		return get_menu_line_count();
	}

	void set_min_max_line_for_selection(uint8_t min_line, uint8_t max_line){
		min_line_for_selection = min_line;
		max_line_for_selection = max_line;
	}

	void process_controller_input(){
		if ( should_move_right() )
			select_next_item();
		else if ( should_move_left() )
			select_previous_item();

		if (user_selected_action())
			raise_menu_selection_event();
		else if (user_cancelled_menu())
			raise_menu_cancel_event();
	}

private:
	bool actions_allowed;
	uint8_t selected_action;
	uint8_t width_in_chars;
	uint8_t scroll_delay;
	const char* const * menu_items;
	uint8_t menu_items_count;
	menu_event_handler *registered_menu_handler;
	uint8_t min_line_for_selection;
	uint8_t max_line_for_selection;

	boolean user_selected_action(){
		return ( gb.buttons.pressed(BTN_A) );
	}

	void raise_menu_selection_event(){
		if ( registered_menu_handler != NULL )
			if ( registered_menu_handler->handle_menu_selection != NULL )
				CALL_REF((*registered_menu_handler), handle_menu_selection)( selected_action );
	}

	boolean user_cancelled_menu(){
		return gb.buttons.pressed(BTN_B);
	}

	void raise_menu_cancel_event(){
		if ( registered_menu_handler != NULL )
			if ( registered_menu_handler->handle_menu_cancel != NULL )
				CALL_REF((*registered_menu_handler), handle_menu_cancel)();
	}

	boolean should_move_right(){
		boolean event_scroll_right = gb.buttons.repeat(BTN_RIGHT, scroll_delay);
		event_scroll_right |= gb.buttons.pressed(BTN_RIGHT);
		return event_scroll_right;
	}

	boolean should_move_left(){
		boolean event_scroll_left = gb.buttons.repeat(BTN_LEFT, scroll_delay);
		event_scroll_left |= gb.buttons.pressed(BTN_LEFT);
		return event_scroll_left;
	}

	void select_next_item(){
		if (selected_action + 1 < menu_items_count)
			selected_action++;
	}

	void select_previous_item(){
		if (selected_action > 0)
			selected_action--;
	}

	const static char * const menuSpaceString = " ";
	const static uint8_t select_boundary_buffer = 2;

	void print_menu_item(const char *item){
		if (gb.display.cursorX == 0){
			gb.display.print( menuSpaceString );
		}
		else{
			uint8_t cur_item_length = strlen(item) + 1;
			uint8_t print_end_x = gb.display.cursorX + cur_item_length*gb.display.fontWidth;
			if (print_end_x > LCDWIDTH){
				gb.display.println();
				gb.display.print( menuSpaceString );
			}
		}
		gb.display.print( item );
		gb.display.print( menuSpaceString );
	}

	void print_selected_menu_item(const char *item){
		uint8_t rect_width = ( strlen(item) + 1 ) * gb.display.fontWidth;
		uint8_t go_back = rect_width + gb.display.fontWidth;
		if (go_back > gb.display.cursorX)
			go_back = gb.display.cursorX;

		uint8_t menu_item_x = gb.display.cursorX - go_back;
		uint8_t menu_item_y = gb.display.cursorY;

		uint8_t cur_screen_x_pos = menu_item_x + (gb.display.fontWidth/2);
		uint8_t cur_screen_y_pos = menu_item_y - 2;
		uint8_t rect_height = gb.display.fontHeight + 3;

		gb.display.drawRoundRect(cur_screen_x_pos, cur_screen_y_pos, rect_width, rect_height, 3);
	}

	uint8_t first_item_print_skipping_lines(uint8_t lines_to_skip){
		uint8_t cur_width = 1;
		uint8_t line_count = 1;
		uint8_t i = 0;

		if (lines_to_skip == 0)
			return 0;

		for (; i < menu_items_count; i++){
			uint8_t cur_action_width = strlen( menu_items[i] ) + 1;
			if (cur_width + cur_action_width <= width_in_chars)
				cur_width += cur_action_width;
			else{
				cur_width = 1 + cur_action_width;
				line_count++;
				if (line_count > lines_to_skip)
					break;
			}
		}
		return i;
	}

	void print_menu(uint8_t start_line, uint8_t lines_to_show){
		uint8_t line_count = 1;
		uint8_t cur_y = gb.display.cursorY;
		uint8_t i = first_item_print_skipping_lines(start_line);
		uint8_t min_visible_line_for_selection = min_line_for_selection + (-start_line);
		uint8_t max_visible_line_for_selection = max_line_for_selection + (-start_line);

		for (; i < menu_items_count && line_count <= lines_to_show; i++){
			print_menu_item( menu_items[i] );

			if (gb.display.cursorY != cur_y){
				line_count++;
				cur_y = gb.display.cursorY;
			}

			if (i == selected_action){
				if (line_count >= min_visible_line_for_selection)
					print_selected_menu_item( menu_items[i] );
				else
					select_next_item();

				if (line_count > max_visible_line_for_selection)
					select_previous_item();
			}
		}
	}

	uint8_t get_menu_line_count(){
		uint8_t cur_width = 1;
		uint8_t line_count = 1;

		for (uint8_t i = 0; i < menu_items_count; i++){
			uint8_t cur_action_width = strlen( menu_items[i] ) + 1;
			if (cur_width + cur_action_width <= width_in_chars)
				cur_width += cur_action_width;
			else{
				cur_width = 1 + cur_action_width;
				line_count++;
			}
		}
		return line_count;
	}

} action_menu;


class word_wrapped_text_box_type {
public:
	word_wrapped_text_box_type()
	: event_description( {0} )
	, event_description_line_count( 0 )
	, description_size_limit( DESCRIPTION_SIZE )
	, width_in_chars( 0 )
	{ }

	void load_event(event& new_event){
		new_event.load_description(event_description, description_size_limit);
		width_in_chars = LCDWIDTH / gb.display.fontWidth;
	}

	uint8_t get_display_line_count(){
		return count_lines_in_description(event_description, width_in_chars);
	}

	void display_portion(uint8_t start_line, uint8_t lines_to_show){
		const char *description_to_show = find_first_line_to_show_from_description( start_line );
		print_string_to_screen_wrapped(description_to_show, lines_to_show, width_in_chars);
	}

private:
	char event_description[DESCRIPTION_SIZE];
	byte event_description_line_count;
	uint8_t description_size_limit;
	uint8_t width_in_chars;

	const char* find_start_of_next_line(const char *string_to_search){
		const char* cur_string_pos = find_next_word(string_to_search);
		uint8_t cur_width = count_chars_to_print_on_one_line(cur_string_pos, width_in_chars);

		return cur_string_pos + cur_width;
	}

	const char* find_first_line_to_show_from_description(uint8_t line_to_find){
		const char* cur_string_pos = event_description;

		for (uint8_t line_count = 0; line_count < line_to_find; line_count++)
			cur_string_pos = find_start_of_next_line(cur_string_pos);

		return cur_string_pos;
	}

	uint8_t get_word_length(const char* text_to_read){
	  bool cont = true;
	  const char* my_char = text_to_read;
	  uint8_t length = 0;

	  while (cont){
	    switch (*my_char){
	      case '\0':
	      case ' ':
	        cont = false;
	        break;
	      default:
	        length++;
	    }
	    my_char++;
	  }
	  return length;
	}

	uint8_t count_chars_to_print_on_one_line(const char* string_to_count, uint8_t max_width_in_chars){
	  bool cont = true;
	  uint8_t cur_line_length = 0;
	  const char *char_to_read_from = string_to_count;

	  while (cont){
	    uint8_t cur_word_length = get_word_length(char_to_read_from);

	    if (cur_word_length == 0){
	      if (*char_to_read_from == '\0')
	        cont = false;
	      else
	        cur_word_length++;
	    }

	    if (cur_line_length + cur_word_length <= max_width_in_chars){
	      cur_line_length += cur_word_length;
	      char_to_read_from += cur_word_length;
	    }
	    else
	      cont = false;
	  }
	  return cur_line_length;
	}

	const char* find_next_word(const char *string_to_search){
	  const char *cur_char = string_to_search;
	  bool cont = true;

	  while (cont){
	    if (*cur_char == '\0' || *cur_char != ' ')
	      cont = false;
	    else
	      cur_char++;
	  }
	  return cur_char;
	}

	void print_string_to_screen_wrapped(const char* string_to_print, uint8_t max_lines, uint8_t max_width_in_chars){
	  const char* cur_string_pos = string_to_print;
	  bool cont = true;
	  uint8_t line_count = 0;
	  char line_to_print[max_width_in_chars + 2];

	  while (cont){
	    cur_string_pos = find_next_word(cur_string_pos);
	    uint8_t cur_width = count_chars_to_print_on_one_line(cur_string_pos, max_width_in_chars);
	    if (cur_width > 0){
	      strncpy(line_to_print, cur_string_pos, cur_width);
	      line_to_print[cur_width] = '\n';
	      line_to_print[cur_width + 1] = '\0';
	      gb.display.print(line_to_print);
	      cur_string_pos += cur_width;
	      line_count++;
	    }
	    else
	      cont = false;
	    if (line_count >= max_lines)
	      cont = false;
	  }
	}

	uint8_t count_lines_in_description(const char* string_to_read, uint8_t max_width_in_chars){
	  const char* cur_string_pos = string_to_read;
	  bool cont = true;
	  uint8_t line_count = 0;

	  while (cont){
	    cur_string_pos = find_next_word(cur_string_pos);
	    uint8_t cur_width = count_chars_to_print_on_one_line(cur_string_pos, max_width_in_chars);
	    if (cur_width > 0){
	      cur_string_pos += cur_width;
	      line_count++;
	    }
	    else
	      cont = false;
	  }
	  return line_count;
	}

} description_box;


class game_screen_type {
public:
	game_screen_type()
	: line_count( 0 )
	, top_line( 0 )
	, screen_height( 0 )
	, screen_width( 0 )
	, scroll_delay( 4 )
	, event_scroll_up( false )
	, event_scroll_down( false )
	, menu_top_line( 0 )
	, menu_title( NULL )
	, select_line( 0 )
	, select_line_limit( 0 )
	, top_line_limit( 0 )
	{}

	void load_event(event& new_event){
		top_line = select_line = 0;
		screen_height = LCDHEIGHT / gb.display.fontHeight;
		screen_width = LCDWIDTH / gb.display.fontWidth;

		description_box.load_event(new_event);
		calculate_line_count();
	}

	void recalculate_line_count(){
		calculate_line_count();
		set_scroll_position(top_line);
		check_and_correct_scroll();
	}

	uint8_t get_scroll_position(){
		return top_line;
	}

	void set_scroll_position(uint8_t new_position){
		top_line = select_line = new_position;
		check_and_correct_scroll();
	}

	void set_menu_title(const __FlashStringHelper* title){
		menu_title = title;
	}

	void update_display(){
		if (should_scroll_down())
			scroll_down();
		if (should_scroll_up())
			scroll_up();
		display_event();
	}

private:
	byte line_count;
	byte top_line;
	byte top_line_limit;
	byte select_line;
	byte select_line_limit;
	byte menu_top_line;
	byte screen_height;
	byte screen_width;
	byte scroll_delay;
	boolean event_scroll_up;
	boolean event_scroll_down;
	const __FlashStringHelper* menu_title;

	void check_and_correct_scroll(){
		if (screen_height >= line_count)
			top_line = select_line = 0;
		else if (top_line > top_line_limit)
			top_line = select_line = top_line_limit;
	}

	void calculate_line_count(){
		uint8_t description_line_count = description_box.get_display_line_count();
		uint8_t menu_headng_line_count = get_menu_heading_display_line_count();
		uint8_t action_menu_line_count = action_menu.get_display_line_count();

		menu_top_line = description_line_count + menu_headng_line_count;
		line_count = description_line_count + menu_headng_line_count + action_menu_line_count + 1;
		top_line_limit = line_count - screen_height;

		uint8_t negative_adjustment_for_menu = 0;
		if (action_menu_line_count >= 3){
			if (action_menu_line_count >= 5)
				negative_adjustment_for_menu = 3;
			else
				negative_adjustment_for_menu = action_menu_line_count + (-2);
		}

		if (top_line_limit > menu_top_line)
			select_line_limit = menu_top_line;
		else
			select_line_limit = top_line_limit;

		select_line_limit += action_menu_line_count + (-1) + (-negative_adjustment_for_menu);
	}

	uint8_t get_menu_heading_display_line_count(){
		if (menu_title != NULL)
			return 3;
		else
			return 1;
	}

	void display_event(){
		display_description();
		display_menu_heading_block();
		display_menu();
	}

	void display_description(){
		description_box.display_portion(top_line, screen_height);
	}

	void display_menu_heading_block(){
		if (menu_title == NULL){
			if (top_line < menu_top_line)
				display_spacer();
		}
		else {
			if (top_line < ( menu_top_line - 2) )
				display_spacer();
			if (top_line < ( menu_top_line - 1 ))
				display_menu_heading();
			if (top_line < menu_top_line)
				display_spacer();
		}
	}

	void display_menu(){
		uint8_t remaining_y_resolution = 0;
		if (gb.display.cursorY < LCDHEIGHT)
			remaining_y_resolution = LCDHEIGHT - gb.display.cursorY;

		uint8_t remaining_lines = remaining_y_resolution / gb.display.fontHeight;

		if (remaining_lines > 0){
			uint8_t start_on_line = 0;
			action_menu.process_controller_input();
			if (top_line > menu_top_line)
				start_on_line = top_line + (-menu_top_line);

			uint8_t min_select_line = start_on_line+1;
			uint8_t max_select_line = start_on_line+1;

			if (select_line >= top_line_limit){
				if (top_line_limit > menu_top_line)
					min_select_line = max_select_line = select_line + (-menu_top_line) + 1;
				else
					min_select_line = max_select_line = select_line + (-top_line_limit) + 1;
			}

			if (remaining_lines >= 4)
				min_select_line = max_select_line += remaining_lines - 3;

			action_menu.set_min_max_line_for_selection( min_select_line, max_select_line );
			display_adjust_for_menu_just_in_view( remaining_lines );
			action_menu.display_portion( start_on_line, remaining_lines );
		}
		// Stupid hack due do doing too much in one frame - button presses would otherwise be detected for other things.
		else
			if (top_line + screen_height <= menu_top_line)
				if (should_jump_to_menu())
					jump_to_menu();
	}

	void display_adjust_for_menu_just_in_view(uint8_t remaining_lines){
		if (remaining_lines == 1){
			if (event_scroll_down)
				scroll_down();
			else if (event_scroll_up)
				scroll_up();
		}
	}

	void display_menu_heading(){
		if (gb.display.cursorY < LCDHEIGHT)
			gb.display.println(menu_title);
	}

	void display_spacer(){
		if (gb.display.cursorY < LCDHEIGHT)
			gb.display.println();
	}

	boolean should_scroll_down(){
		event_scroll_down = gb.buttons.repeat(BTN_DOWN, scroll_delay);
		event_scroll_down |= gb.buttons.pressed(BTN_DOWN);
		return event_scroll_down;
	}

	boolean should_scroll_up(){
		event_scroll_up = gb.buttons.repeat(BTN_UP, scroll_delay);
		event_scroll_up |= gb.buttons.pressed(BTN_UP);
		return event_scroll_up;
	}

	boolean should_jump_to_menu(){
		boolean jump = gb.buttons.pressed(BTN_A);
		jump |= gb.buttons.pressed(BTN_B);
		return jump;
	}

	void jump_to_menu(){
		if (top_line_limit < menu_top_line)
			top_line = select_line = top_line_limit;
		else
			top_line = select_line = menu_top_line + (-3);
	}

	void scroll_down(){
		if (select_line < select_line_limit){
			if (select_line < top_line_limit)
				top_line = select_line + 1;

			select_line++;
		}
	}

	void scroll_up(){
		if (select_line > 0){
			if (select_line <= top_line_limit)
				top_line = select_line + (-1);

			select_line--;
		}
	}
} game_screen;

class game_presenter_type : public menu_event_handler{
public:
	void init(){
		load_first_event();
		action_menu.register_menu_handler(this);
		switch_to_event();
	}

	void update(){
		game_screen.update_display();
	}

private:
	event events[EVENT_MEMORY];
	uint8_t event_stack_pos;
	char object_menu_buf[OBJECT_MENU_LENGTH][OBJECT_MENU_ITEM_LENGTH];
	char *object_menu[OBJECT_MENU_LENGTH];
	uint8_t object_menu_length;
	action_id selected_action;
	player_item_id selected_item;
	uint8_t events_scroll_pos[EVENT_MEMORY];

	void load_first_event(){
		event_stack_pos = 0;
		game_screen.set_scroll_position(0);
		starting_event first_event;
		load_new_event( first_event );
	}

	void switch_to_event(){
		game_screen.load_event(get_current_event());
		switch_to_action_menu();
		game_screen.set_scroll_position(get_current_saved_screen_scroll_position());
	}

	void switch_to_action_menu(){
		if (get_current_event().actions_are_allowed()){
			game_screen.set_menu_title(F("Select action"));
			uint8_t menu_list_length = sizeof(standard_actions)/sizeof(typeof(standard_actions[0]));
			action_menu.load_menu_from_string_list(standard_actions, menu_list_length);
			handle_menu_selection = static_cast<menu_selection_handler>( &handle_action_menu_selection );
			handle_menu_cancel = NULL;
		}
		else {
			game_screen.set_menu_title(NULL);
			uint8_t menu_list_length = sizeof(continue_actions)/sizeof(typeof(continue_actions[0]));
			action_menu.load_menu_from_string_list(continue_actions, menu_list_length);
			handle_menu_selection = static_cast<menu_selection_handler>( &handle_action_menu_continue_selected );
			handle_menu_cancel = static_cast<menu_cancel_handler>( &handle_action_menu_continue );
		}
		game_screen.recalculate_line_count();
	}

	void save_current_screen_scoll_position(){
		events_scroll_pos[event_stack_pos] = game_screen.get_scroll_position();
	}

	void clear_current_saved_screen_scroll_position(){
		events_scroll_pos[event_stack_pos] = 0;
	}

	uint8_t get_current_saved_screen_scroll_position(){
		return events_scroll_pos[event_stack_pos];
	}

	void load_new_event(event& new_event){
		save_current_screen_scoll_position();
		( ++event_stack_pos ) %= EVENT_MEMORY;
		events[event_stack_pos] = new_event;
		clear_current_saved_screen_scroll_position();
	}

	void load_previous_event(){
		( --event_stack_pos ) %= EVENT_MEMORY;
	}

	event& get_current_event(){
		return events[event_stack_pos];
	}

	void handle_object_for_item_menu_selection(uint8_t selection){
		event& current_event = get_current_event();
		event next_event = current_event.process_item_on_object(selected_item, selection);
		load_new_event(next_event);
		switch_to_event();
	}

	void handle_object_menu_selection(uint8_t selection){
		event& current_event = get_current_event();
		event next_event = current_event.process_action_on_object(selected_action, selection);
		load_new_event(next_event);
		switch_to_event();
	}

	void handle_item_menu_selection(uint8_t selection){
		selected_item = player.get_item_by_index(selection);
		switch_to_object_for_item_menu();
	}

	void handle_action_menu_selection(uint8_t action_selected){
		if (action_selected == ACTION_ID_ITEM)
			switch_to_item_menu();
		else
			switch_to_object_menu();
		selected_action = action_selected;
	}

	void handle_action_menu_continue(){
		if (get_current_event().should_return_to_previous_event())
			load_previous_event();
		else{
			event next_event = get_current_event().get_continue_event();
			load_new_event(next_event);
		}
		switch_to_event();
	}

	void handle_action_menu_continue_selected(uint8_t ignored){
		handle_action_menu_continue();
	}

	void handle_object_menu_cancel(){
		switch_to_action_menu();
	}

	void switch_to_object_for_item_menu(){
		game_screen.set_menu_title(F("On what?"));
		load_object_menu();
		action_menu.load_menu_from_string_list(object_menu, object_menu_length);
		game_screen.recalculate_line_count();
		handle_menu_selection = static_cast<menu_selection_handler>( &handle_object_for_item_menu_selection );
		handle_menu_cancel = static_cast<menu_cancel_handler>( &handle_object_menu_cancel );
	}

	void switch_to_object_menu(){
		game_screen.set_menu_title(F("Which object?"));
		load_object_menu();
		action_menu.load_menu_from_string_list(object_menu, object_menu_length);
		game_screen.recalculate_line_count();
		handle_menu_selection = static_cast<menu_selection_handler>( &handle_object_menu_selection );
		handle_menu_cancel = static_cast<menu_cancel_handler>( &handle_object_menu_cancel );
	}

	void switch_to_item_menu(){
		clear_object_menu();
		uint8_t menu_length = player.load_item_menu(&object_menu_buf[0][0], OBJECT_MENU_LENGTH, OBJECT_MENU_ITEM_LENGTH);
		build_object_menu_refs();
		if (menu_length > 0){
			game_screen.set_menu_title(F("Use what?"));
			action_menu.load_menu_from_string_list(object_menu, menu_length);
			game_screen.recalculate_line_count();
			handle_menu_selection = static_cast<menu_selection_handler>( &handle_item_menu_selection );
			handle_menu_cancel = static_cast<menu_cancel_handler>( &handle_object_menu_cancel );
		}
		else{
			event empty_item_menu(F("You are carrying no useful items!"));
			load_new_event(empty_item_menu);
			switch_to_event();
		}
	}

	void clear_object_menu(){
		memset(object_menu_buf, 0, sizeof(object_menu_buf));
	}

	void load_object_menu_buf(){
		clear_object_menu();
		event& current_event = get_current_event();
		current_event.load_object_menu(&object_menu_buf[0][0], OBJECT_MENU_LENGTH, OBJECT_MENU_ITEM_LENGTH);
	}

	void load_object_menu(){
		load_object_menu_buf();
		build_object_menu_refs();
	}

	void build_object_menu_refs(){
		uint8_t i=0;
		for(; i<OBJECT_MENU_LENGTH; i++){
			if (object_menu_buf[i][0] != '\0')
				object_menu[i] = &object_menu_buf[i][0];
			else
				break;
		}
		object_menu_length = i;
	}
} game_presenter;

void setup() {
  gb.begin();
  Serial.begin(115200);
}

void loop() {
	if (gb.update()){
		switch (game_state.state){
		case GAME_STATE_ID_INIT:
			game_presenter.init();
			game_state.state = GAME_STATE_ID_PLAY;
			break;
		case GAME_STATE_ID_PLAY:
			game_presenter.update();
			if (gb.buttons.pressed(BTN_C))
				game_state.state = GAME_STATE_ID_TITLE;
			break;
		case GAME_STATE_ID_TITLE:
			gb.display.setFont(font3x5);
		    gb.titleScreen(F("The Cursed Tower"));
		    game_state.state = GAME_STATE_ID_INIT;
			gb.display.setFont(font5x7);
			break;
		}
	}
}
