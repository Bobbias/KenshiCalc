#include "Walnut/Application.h"
#include "Walnut/EntryPoint.h"

#include "Walnut/Image.h"

#include <cstdlib>
//#include <cstdio>

#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <frozen/unordered_map.h>
#include <frozen/unordered_set.h>

#include <winsqlite/winsqlite3.h>

// utility wrappers from https://bryanstamour.com/post/2017-03-12-sqlite-with-cpp/

// custom deleter type
template <typename Func> struct scope_exit {
    explicit scope_exit(Func f) : f_{f} {}
    ~scope_exit() { f_(); }
private:
    Func f_;
};

struct sqlite3_deleter {
    void operator () (sqlite3* db) const { sqlite3_close(db); }
};

// wrap the sqlite3 handle object in a unique_ptr and use the above deleter to ensure it's closed correctly for us
using sqlite3_handle = std::unique_ptr<sqlite3, sqlite3_deleter>;

// helper function to get a unique_ptr handle
auto make_sqlite3_handle(char const* db_name)
{
    sqlite3* p;
    int rc = sqlite3_open_v2(db_name, &p, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
	if (rc == SQLITE_OK)
	{
		std::cout << "db opened successfully\n";
	}
	else if (rc == SQLITE_ERROR)
	{
		std::cout << "db open error\n";
	}
    sqlite3_handle h{p};
    if (rc) h.reset();
    return h;
}

// the callback for processing results from a query
int get_names_callback(void* result_vector, int col_count, char** row_data, char** col_names)
{
	std::vector<std::string> *names = static_cast<std::vector<std::string> *>(result_vector);
	fprintf(stderr, "Got: %s\n", row_data[0]);
	if (row_data != nullptr)
		names->push_back(std::string(row_data[0]));
    
    return 0;
}

int get_tables_callback(void* result_vector, int col_count, char** row_data, char** col_names)
{
	std::vector<std::string> *names = static_cast<std::vector<std::string> *>(result_vector);
	for (int i = 0; i < col_count; ++i)
	{
		fprintf(stderr, "%s = %s\n", col_names[i], row_data[i] ? row_data[i] : "NULL");
	}
	return 0;
}

int get_weapon_data_callback(void* result_vector, int col_count, char** row_data, char** col_names)
{
	return 0;
}

std::string GetSQLITEReturnType(int rc)
{
	switch (rc) {
	case 0: return "SQLITE_OK";
	case 1: return "SQLITE_ERROR";
	case 21: return "SQLITE_MISUSE";
	case 25: return "SQLITE_RANGE";
	case 101: return "SQLITE_DONE";
	default: return "UNKNOWN";
	}
}


char const *const tables_sql = "SELECT name FROM sqlite_schema WHERE type ='table' AND name NOT LIKE 'sqlite_%'";

char const* const weapon_name_sql    = "SELECT name FROM WeaponName;";
char const* const weapon_class_sql   = "SELECT name FROM WeaponClass;";
char const* const weapon_quality_sql = "SELECT name FROM WeaponQuality;";
char const* const weapon_image_sql   = "SELECT path FROM WeaponImage;";

char const* const weapon_by_name_sql = "SELECT * FROM Weapon WHERE name='?'";
char const* weapon_names_by_type_sql = "SELECT name FROM WeaponNamesByClass WHERE type=:class";


struct MyApplicationSpecification : Walnut::ApplicationSpecification {
		std::string Name = "Walnut App";
		uint32_t Width = 1600;
		uint32_t Height = 900;
};

struct Weapon {
	std::string name;
	std::string type;
	float cut_damage;
	float blunt_damage;
};

class ExampleLayer : public Walnut::Layer
{
public:
	~ExampleLayer() override
	{
		sqlite3_free(m_error_msg);
		if (!(sqlite3_close_v2(m_db.get()) == SQLITE_OK))
		{
			std::cerr << "SQLITE error: " << m_error_msg << '\n';
		}
	}

	void OnAttach() override
	{
		m_db = make_sqlite3_handle("KenshiData");
		if (!m_db) {
			std::cerr << "Can't open database: " << sqlite3_errmsg(m_db.get()) << '\n';
		}
		/*int rc = sqlite3_exec(db.get(), tables_sql, get_tables_callback, &table_names, &error_msg);
		fprintf(stderr, "return code from tables query = %d\n", rc);
		if (rc != SQLITE_OK)
		{
			std::cerr << "SQLITE error: " << error_msg << '\n';
		}*/
		

		/*int rc = sqlite3_exec(m_db.get(), weapon_name_sql, get_names_callback, &m_weapon_names, &error_msg);
		if (rc != SQLITE_OK)
		{
			std::cerr << "SQLITE error: " << error_msg << '\n';
		}*/
		int rc = sqlite3_exec(m_db.get(), weapon_class_sql, get_names_callback, &m_weapon_types, &m_error_msg);
		if (rc != SQLITE_OK)
		{
			std::cerr << "SQLITE error: " << m_error_msg << '\n';
		}
		rc = sqlite3_exec(m_db.get(), weapon_quality_sql, get_names_callback, &m_weapon_qualities, &m_error_msg);
		if (rc != SQLITE_OK)
		{
			std::cerr << "SQLITE error: " << m_error_msg << '\n';
		}
		rc = sqlite3_exec(m_db.get(), weapon_image_sql, get_names_callback, &m_weapon_images, &m_error_msg);
		if (rc != SQLITE_OK)
		{
			std::cerr << "SQLITE error: " << m_error_msg << '\n';
		}
		//enable extended result codes
		rc = sqlite3_extended_result_codes(m_db.get(), 1);
		if (rc == SQLITE_OK)
		{
			std::cerr << "Extended result codes enabled.\n";
		}
	}

	void OnUIRender() override
	{
		// Helper values
		const float  f32_zero = 0.f;
		const float  f32_one = 1.f;
		const float  f32_lo_a = -10000000000.0f;
		const float  f32_hi_a = +10000000000.0f;

		static bool  show_result = false;
		
		// Weapon stats
		static float f32_cut_damage = 0.123f;
		static float f32_blunt_damage = 0.123f;

		// Player stats
		static ImU8  u8_strength = 1;
		static ImU8  u8_dexterity = 1;
		static ImU8  u8_weapon_skill = 1;
		const  ImU8  u8_one = 1;

		// Target stats
		static float f32_racial_multiplier = 1.f;

		// FCS values
		const  ImU8  u8_MinDamage = 20;
		const  ImU8  u8_MaxDamage = 80;
		const  float f32_damage_multiplier = 0.65f;

		// result
		static float f32_result = 0.f;


		// Primary damage:
		// Cut:
		// ((
		// (
		// (((MaxDamage - MinDamage) * 0.5) * (Dexlevel * 0.01)) +
		// (((MaxDamage - MinDamage) * 0.5) * (Weplevel * 0.01))) * WeaponMultiplier)
		// + Min Damage) * FCSDamageMultiplier * RacialMultiplier * GlobalDamageModifier * (AttackPower * 0.01)
		// 
		// Blunt: ((((((MaxDamage - MinDamage) * 0.75) * (Strlevel * 0.01)) + (((MaxDamage - MinDamage) * 0.25) * (Weplevel * 0.01))) * WeaponMultiplier) + Min Damage) * FCSDamageMultiplier * RacialMultiplier * GlobalDamageModifier * (AttackPower * 0.01)
		//Secondary Damage:
		// Cut: ((((((Max Damage - (Maxdamage * 0.1)) * 0.5) * (Dexlevel * 0.01)) + (((Max Damage - (Maxdamage * 0.1)) * 0.5) * (Weplevel * 0.01))) * Weapon Multiplier) + (Maxdamage * 0.1)) * FCSDamageMultiplier * RacialMultiplier * GlobalDamageModifier * (AttackPower * 0.01)
		// Blunt: ((((MaxDamage * 0.75) * (Strlevel * 0.01)) + ((MaxDamage * 0.25) * (Weplevel * 0.01))) * WeaponMultiplier) * FCSDamageMultiplier * RacialMultiplier * GlobalDamageModifier * (AttackPower * 0.01)
		//
		// Min/Max damage. These values are in the FCS as "Cut Damage 1, Cut Damage 99, Blunt Damage 1, and Blunt Damage 99".
		// By default the min and max values for both damage types are 20 and 80, respectively.
		// They are designed to represent the damage values assuming all multipliers are at 1, and the related skills are at 1 (min) and 100 (max).
		// A lot of what you see in the formulas involving them is getting the difference between them, since most math involves working off that.
		// This is also why the minimum is added back in later in most formulas. The secondary formulas also play off max damage in unusual ways.
		// With cut using 10% of the max damage as its min damage value, and blunt actually using 0 as its min damage value
		// (hence why it was removed altogether from the formula, since doing math with 0 is pointless).
		// 
		// DexLevel, StrLevel, and WepLevel. These simply represent your level in the respective stats. They are multiplied by 0.01 to get a good value to multiply off of easily.
		// 
		// WeaponMultiplier is simply the damage values that show on the weapon.
		// 
		// FCSDamageMultiplier is a damage modifier found in the global settings in the FCS. By default it will be a 0.65, though a mod or game update could change it.
		// 
		// RacialMultiplier is the modifiers based on race, things like +50% to Beak things, etc.
		// 
		// GlobalDamageModifier is the slider found on new game/import
		// 
		// And finally AttackPower is a variable on the combat animations themselves. Most of the time this will stay at 100, though there are cases where it changes.
		// It is multiplied by 0.01 to get a good number to multiply off of easily.

		auto cut_primary = [&] { return ((((((u8_MaxDamage - u8_MinDamage) * 0.5) * (u8_dexterity * 0.01)) +
			(((u8_MaxDamage - u8_MinDamage) * 0.5) * (u8_weapon_skill * 0.01))) * f32_cut_damage) + u8_MinDamage) * f32_damage_multiplier * f32_racial_multiplier * 1 * (100 * 0.01);
		};
		auto blunt_primary = [&] { return ((((((u8_MaxDamage - u8_MinDamage) * 0.75) * (u8_strength * 0.01)) +
			(((u8_MaxDamage - u8_MinDamage) * 0.25) * (u8_weapon_skill * 0.01))) * f32_blunt_damage) + u8_MinDamage) * f32_damage_multiplier * f32_racial_multiplier * 1 * (100 * 0.01);
		};

		// Manually input weapon/player stats
		ImGui::Begin("Manual Damage Calculator");

		if(ImGui::CollapsingHeader("Weapon"))
		{
			ImGui::InputScalar("Cut Damage", ImGuiDataType_Float,  &f32_cut_damage, &f32_one);
			ImGui::InputScalar("Blunt Damage", ImGuiDataType_Float,  &f32_blunt_damage, &f32_one);
		}

		if (ImGui::CollapsingHeader("Character"))
		{
			ImGui::InputScalar("Strength", ImGuiDataType_U8,  &u8_strength, &u8_one);
			ImGui::InputScalar("Dexterity", ImGuiDataType_U8,  &u8_dexterity, &u8_one);
			ImGui::InputScalar("Weapon Skill", ImGuiDataType_U8,  &u8_weapon_skill, &u8_one);
		}

		if(ImGui::Button("Calculate"))
		{
			if (f32_cut_damage > f32_blunt_damage)
				f32_result = cut_primary();
			else
				f32_result = blunt_primary();
			show_result = true;
		}
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			show_result = false;
		}

		if(show_result)
		{
			ImGui::Text("Result: %f", f32_result);
		}

		ImGui::End();

		// Select weapon from list of ingame weapons.
		ImGui::Begin("Select Weapon");
		
        static int current_weapon_type_index = 0;
		static int prev_weapon_type_index = -1;

		if (ImGui::BeginListBox("Class")) {
			for (auto n = m_weapon_types.begin(); n < m_weapon_types.end(); ++n)
			{
				auto current_index = n - m_weapon_types.begin();
				const bool is_selected = (current_weapon_type_index == current_index);
				if (ImGui::Selectable(n->c_str(), is_selected))
				{
					current_weapon_type_index = current_index;
					// if the user has selected a different item in the list
					if (prev_weapon_type_index == -1 || prev_weapon_type_index != current_weapon_type_index)
					{
						// update weapons list
						m_weapon_names = GetWeaponNamesByType(*n);
						prev_weapon_type_index = current_weapon_type_index;
					}
				}

				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}

		static int current_weapon_index = 0;
		static int prev_weapon_index = -1;
		if (ImGui::BeginListBox("Weapon"))
		{
			for (auto n = m_weapon_names.begin(); n < m_weapon_names.end(); ++n)
			{
				auto current_index = n - m_weapon_names.begin();
				const bool is_selected = (current_weapon_index == current_index);
				std::string str = *n;
				std::transform(n->begin(), n->end(), str.begin(), [](char c) { if (c == '_') return ' '; else return c; });

				if (ImGui::Selectable(str.c_str(), is_selected))
				{
					current_weapon_index = current_index;
					if (prev_weapon_index == -1 || prev_weapon_index != current_weapon_index)
					{
						// todo: get weapon data here
						prev_weapon_index = current_weapon_index;
					}
				}
				
				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}

		static int current_quality_index = 0;
		static int prev_quality_index = -1;
		if (ImGui::BeginListBox("Quality"))
		{
			for (auto n = m_weapon_qualities.begin(); n < m_weapon_qualities.end(); ++n)
			{
				auto current_index = n - m_weapon_qualities.begin();
				const bool is_selected = (current_quality_index == current_index);
				std::string str = *n;
				//std::transform(n->begin(), n->end(), str.begin(), [](char c) { if (c == '_') return ' '; else return c; });
				if (ImGui::Selectable(str.c_str(), is_selected))
				{
					current_quality_index = current_index;
					if (prev_quality_index == -1 || prev_quality_index != current_quality_index)
					{
						// todo: get weapon data here
						prev_quality_index = current_quality_index;
					}
				}
				
				// Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}
		

		ImGui::End();

		//ImGui::ShowDemoWindow();
	}
	private:
		std::vector<std::string> m_weapon_types{};
		std::vector<std::string> m_weapon_names{};
		std::vector<std::string> m_weapon_qualities{};
		std::vector<std::string> m_weapon_images{};
		sqlite3_handle m_db{};
		char* m_error_msg{};

	void check_and_print_error_message(int ret)
	{
		if (m_error_msg != nullptr)
			std::cerr << "SQLITE error: " << m_error_msg << '\n';
		else
		{
			std::cerr << "SQLITE error: An additional error occurred while retrieving error message (message was null)\n";
			std::cerr << "SQLITE error: the result code was: " << ret << " = " << GetSQLITEReturnType(ret) << "\n";
		}
	}

	std::vector<std::string> GetWeaponNamesByType(std::string const& type_name)
	{
		std::vector<std::string> weapon_names;
		sqlite3_stmt* statement;
		std::cerr << "Preparing statement\n";
		int ret = sqlite3_prepare_v2(m_db.get(), weapon_names_by_type_sql, -1, &statement, NULL); // strlen(weapon_names_by_type_sql)
		if (ret != SQLITE_OK)
		{
			check_and_print_error_message(ret);
			sqlite3_finalize(statement);
			sqlite3_clear_bindings(statement);
			return weapon_names;
		}
		std::cerr << "Binding SQL statement\n";
		int idx = sqlite3_bind_parameter_index(statement, ":class");
		std::cerr << "Binding `" << type_name << "` to index " << idx << " of " << sqlite3_bind_parameter_count(statement) << '\n';
		ret = sqlite3_bind_text(statement, idx, type_name.c_str(), -1, SQLITE_TRANSIENT);
		if (ret != SQLITE_OK)
		{
			check_and_print_error_message(ret);
			sqlite3_finalize(statement);
			sqlite3_clear_bindings(statement);
			return weapon_names;
		}
		std::cerr << "Stepping first time\n";
		do {
			ret = sqlite3_step(statement);
			if (ret == SQLITE_DONE)
			{
				break;
			}
			else if (ret != SQLITE_OK && ret != SQLITE_ROW)
			{
				check_and_print_error_message(ret);
				sqlite3_finalize(statement);
				sqlite3_clear_bindings(statement);
				return weapon_names;
			}
			//convert unsigned char* to standard c string type for conversion to std::string. This will break if the result contains null bytes (shouldn't happen here!).
			std::cerr << "number of columns in result: " << sqlite3_column_count(statement) << '\n';
			std::cerr << "getting column 0 value\n";
			if (auto result = sqlite3_column_text(statement, 0); result != nullptr) {
				std::cerr << "result: `" << result << "`\n";
				auto str = std::string(reinterpret_cast<char const*>(result));
				weapon_names.push_back(str);
			}
			else
			{
				std::cerr << "no result after step!\n";
				continue;
			}
		} while (ret == SQLITE_ROW);
		sqlite3_clear_bindings(statement);
		sqlite3_finalize(statement);
		return weapon_names;
	}
};

Walnut::Application* Walnut::CreateApplication(int argc, char** argv)
{	
	MyApplicationSpecification spec;
	spec.Name = "KenshiCalc";

	auto table_names = std::vector<std::string>();

	auto* app = new Walnut::Application(spec);
	app->PushLayer<ExampleLayer>();
	app->SetMenubarCallback([app]()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit"))
			{
				app->Close();
			}
			ImGui::EndMenu();
		}
	});
	return app;
}