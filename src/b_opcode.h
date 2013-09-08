#ifndef B_OPCODE_H
#define B_OPCODE_H

/*

   List of instructions supported by the engine.

*/

typedef enum {
   // 0
   pc_none,
   pc_terminate,
   pc_suspend,
   pc_push_number,
   pc_lspec1,
   pc_lspec2,
   pc_lspec3,
   pc_lspec4,
   pc_lspec5,
   pc_lspec1_direct,

   // 10
   pc_lspec2_direct,
   pc_lspec3_direct,
   pc_lspec4_direct,
   pc_lspec5_direct,
   pc_add,
   pc_sub,
   pc_mul,
   pc_div,
   pc_mod,
   pc_eq,

   // 20
   pc_ne,
   pc_lt,
   pc_gt,
   pc_le,
   pc_ge,
   pc_assign_script_var,
   pc_assign_map_var,
   pc_assign_world_var,
   pc_push_script_var,
   pc_push_map_var,

   // 30
   pc_push_world_var,
   // Compound assignment.
   pc_add_script_var,
   pc_add_map_var,
   pc_add_world_var,
   pc_sub_script_var,
   pc_sub_map_var,
   pc_sub_world_var,
   pc_mul_script_var,
   pc_mul_map_var,
   pc_mul_world_var,

   // 40
   pc_div_script_var,
   pc_div_map_var,
   pc_div_world_var,
   pc_mod_script_var,
   pc_mod_map_var,
   pc_mod_world_var,
   pc_inc_script_var,
   pc_inc_map_var,
   pc_inc_world_var,
   pc_dec_script_var,

   // 50
   pc_dec_map_var,
   pc_dec_world_var,
   pc_goto,
   pc_if_goto,
   pc_drop, // Pop stack.
   pc_delay,
   pc_delay_direct,
   pc_random,
   pc_random_direct,
   pc_thing_count,

   // 60
   pc_thing_count_direct,
   pc_tag_wait,
   pc_tag_wait_direct,
   pc_poly_wait,
   pc_poly_wait_direct,
   pc_change_floor,
   pc_change_floor_direct,
   pc_change_ceiling,
   pc_change_ceiling_direct,
   pc_restart,

   // 70
   pc_and_logical,
   pc_or_logical,
   pc_and_bitwise,
   pc_or_bitwise,
   pc_eor_bitwise,
   pc_negate_logical,
   pc_lshift,
   pc_rshift,
   pc_unary_minus,
   pc_if_not_goto,

   // 80
   pc_line_side,
   pc_script_wait,
   pc_script_wait_direct,
   pc_clear_line_special,
   pc_case_goto,
   pc_begin_print,
   pc_end_print,
   pc_print_string,
   pc_print_number,
   pc_print_character,

   // 90
   pc_player_count,
   pc_game_type,
   pc_game_skill,
   pc_timer,
   pc_sector_sound,
   pc_ambient_sound,
   pc_sound_sequence,
   pc_set_line_texture,
   pc_set_line_blocking,
   pc_set_line_special,

   // 100
   pc_thing_sound,
   pc_end_print_bold,
   pc_activator_sound,
   pc_lpcal_ambient_sound,
   pc_set_line_monster_blocking,
   pc_player_blue_skull,
   pc_player_red_skull,
   pc_player_yellow_skull,
   pc_player_master_skull,
   pc_player_blue_card,

   // 110
   pc_player_red_card,
   pc_player_yellow_card,
   pc_player_master_card,
   pc_player_black_skull,
   pc_player_silver_skull,
   pc_player_gold_skull,
   pc_player_black_card,
   pc_player_silver_card,
   pc_player_on_team,
   pc_player_team,

   // 120
   pc_player_health,
   pc_player_armor_points,
   pc_player_frags,
   pc_player_expert,
   pc_blue_team_count,
   pc_red_team_count,
   pc_blue_team_score,
   pc_red_team_score,
   pc_is_one_flag_ctf,
   pc_get_invasion_wave,

   // 130
   pc_get_invastion_state,
   pc_print_name,
   pc_music_change,
   pc_console_command_direct,
   pc_console_command,
   pc_single_player,
   pc_fixed_mul,
   pc_fixed_div,
   pc_set_gravity,
   pc_set_gravity_direct,

   // 140
   pc_set_air_control,
   pc_set_air_control_direct,
   pc_clear_inventory,
   pc_give_inventory,
   pc_give_inventory_direct,
   pc_take_inventory,
   pc_take_inventory_direct,
   pc_check_inventory,
   pc_check_inventory_direct,
   pc_spawn,

   // 150
   pc_spawn_direct,
   pc_spawn_spot,
   pc_spawn_spot_direct,
   pc_set_music,
   pc_set_music_direct,
   pc_local_set_music,
   pc_local_set_music_direct,
   pc_print_fixed,
   pc_print_localized,
   pc_more_hud_message,

   // 160
   pc_opt_hud_message,
   pc_end_hud_message,
   pc_end_hud_message_bold,
   pc_set_style,
   pc_set_style_direct,
   pc_set_font,
   pc_set_font_direct,
   pc_push_byte,
   pc_lspec1_direct_b,
   pc_lspec2_direct_b,

   // 170
   pc_lspec3_direct_b,
   pc_lspec4_direct_b,
   pc_lspec5_direct_b,
   pc_delay_direct_b,
   pc_random_direct_b,
   pc_push_bytes,
   pc_push2_bytes,
   pc_push3_bytes,
   pc_push4_bytes,
   pc_push5_bytes,

   // 180
   pc_set_thing_special,
   pc_assign_global_var,
   pc_push_global_var,
   pc_add_global_var,
   pc_sub_global_var,
   pc_mul_global_var,
   pc_div_global_var,
   pc_mod_global_var,
   pc_inc_global_var,
   pc_dec_global_var,

   // 190
   pc_fade_to,
   pc_fade_range,
   pc_cancel_fade,
   pc_play_movie,
   pc_set_floor_trigger,
   pc_set_ceiling_trigger,
   pc_get_actor_x,
   pc_get_actor_y,
   pc_get_actor_z,
   pc_start_translation,

   // 200
   pc_translation_range1,
   pc_translation_range2,
   pc_end_translation,
   pc_call,
   pc_call_discard,
   pc_return_void,
   pc_return_val,
   pc_push_map_array,
   pc_assign_map_array,
   pc_add_map_array,

   // 210
   pc_sub_map_array,
   pc_mul_map_array,
   pc_div_map_array,
   pc_mod_map_array,
   pc_inc_map_array,
   pc_dec_map_array,
   pc_dup,
   pc_swap,
   pc_write_to_ini,
   pc_get_from_ini,

   // 220
   pc_sin,
   pc_cos,
   pc_vector_angle,
   pc_check_weapon,
   pc_set_weapon,
   pc_tag_string,
   pc_push_world_array,
   pc_assign_world_array,
   pc_add_world_array,
   pc_sub_world_array,

   // 230
   pc_mul_world_array,
   pc_div_world_array,
   pc_mod_world_array,
   pc_inc_world_array,
   pc_dec_world_array,
   pc_push_global_array,
   pc_assign_global_array,
   pc_add_global_array,
   pc_sub_global_array,
   pc_mul_global_array,

   // 240
   pc_div_global_array,
   pc_mod_global_array,
   pc_inc_global_array,
   pc_dec_global_array,
   pc_set_marine_weapon,
   pc_set_actor_property,
   pc_get_actor_property,
   pc_player_number,
   pc_activator_tid,
   pc_set_marine_sprite,

   // 250
   pc_get_screen_width,
   pc_get_screen_height,
   pc_thing_projectile2,
   pc_str_len,
   pc_get_hud_size,
   pc_get_cvar,
   pc_case_goto_sorted,
   pc_set_result_value,
   pc_get_line_row_offset,
   pc_get_actor_floor_z,

   // 260
   pc_get_actor_angle,
   pc_get_sector_floor_z,
   pc_get_sector_ceiling_z,
   pc_lspec5_result,
   pc_get_sigil_pieces,
   pc_get_level_info,
   pc_change_sky,
   pc_player_in_game,
   pc_player_is_bot,
   pc_set_camera_to_texture,

   // 270
   pc_end_log,
   pc_get_ammo_capacity,
   pc_set_ammo_capacity,
   pc_print_map_char_array,
   pc_print_world_char_array,
   pc_print_global_char_array,
   pc_set_actor_angle,
   pc_grab_input,
   pc_set_mouse_pointer,
   pc_move_mouse_pointer,

   // 280
   pc_spawn_projectile,
   pc_get_sector_light_level,
   pc_get_actor_ceiling_z,
   pc_get_actor_position_z,
   pc_clear_actor_inventory,
   pc_give_actor_inventory,
   pc_take_actor_inventory,
   pc_check_actor_inventory,
   pc_thing_count_name,
   pc_spawn_spot_facing,

   // 290
   pc_player_class,
   pc_and_script_var,
   pc_and_map_var,
   pc_and_world_var,
   pc_and_global_var,
   pc_and_map_array,
   pc_and_world_array,
   pc_and_global_array,
   pc_eor_script_var,
   pc_eor_map_var,

   // 300
   pc_eor_world_var,
   pc_eor_global_var,
   pc_eor_map_array,
   pc_eor_world_array,
   pc_eor_global_array,
   pc_or_script_var,
   pc_or_map_var,
   pc_or_world_var,
   pc_or_global_var,
   pc_or_map_array,

   // 310
   pc_or_world_array,
   pc_or_global_array,
   pc_ls_script_var,
   pc_ls_map_var,
   pc_ls_world_var,
   pc_ls_global_var,
   pc_ls_map_array,
   pc_ls_world_array,
   pc_ls_global_array,
   pc_rs_script_var,

   // 320
   pc_rs_map_var,
   pc_rs_world_var,
   pc_rs_global_var,
   pc_rs_map_array,
   pc_rs_world_array,
   pc_rs_global_array,
   pc_get_player_info,
   pc_change_level,
   pc_sector_damage,
   pc_replace_textures,

   // 330
   pc_negate_binary,
   pc_get_actor_pitch,
   pc_set_actor_pitch,
   pc_print_bind,
   pc_set_actor_state,
   pc_thing_damage2,
   pc_use_inventory,
   pc_use_actor_inventory,
   pc_check_actor_ceiling_texture,
   pc_check_actor_floor_texture,

   // 340
   pc_get_actor_light_level,
   pc_set_mugshot_state,
   pc_thing_count_sector,
   pc_thing_count_name_sector,
   pc_check_player_camera,
   pc_morph_actor,
   pc_unmorph_actor,
   pc_get_player_input,
   pc_classify_actor,
   pc_print_binary,

   // 350
   pc_print_hex,
   pc_call_func,
   pc_save_string,

   // Pseudo instructions. These are used to simplify and/or improve code in
   // the compiler.
   // -----------------------------------------------------------------------

   pp_start = 2000000,
   // Only output the opcode of an instruction.
   pp_opcode,
   // Output an argument for the current instruction.
   pp_arg,
   pp_arg_pos,
   // Format: <storage> <index> <assign-op>
   pp_assign_var,
   pp_assign_array,
   // Same as the real instructions but with an iseq_pos_t argument.
   pp_goto,
   pp_if_goto,
   pp_if_not_goto,
} opcode_t;

#endif