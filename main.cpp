#include <map>
#include <vector>
#include <iostream>
#include <cassert>


int _next_id() 
{ 
    static int _counter = 0;
    return _counter++; 
}


enum ROTATE { LEFT, RIGHT };
enum SIDE { BOTTOM, TOP };
class unit;
class unit_class;


struct item_use_context 
{
    virtual ~item_use_context() = default;
    virtual void damage_target(const int dmg) = 0;
    virtual void next_target(ROTATE rotate, int dist = 1) = 0;
    virtual void rotate_self(ROTATE rotate, int dist = 1) = 0;
    virtual void swap_self(ROTATE rotate, int dist = 1) = 0;
};


struct item
{
    virtual ~item() = default;
    virtual void run(item_use_context &ctx) = 0;
};


struct unit
{
    int _unit_class_id = -1;
    int _hp = -1;
    int _debug_instance_id = -1;
};


struct unit_class
{
    std::string _debug_name = "";
    int _hp = -1;
    void (*_run)(item_use_context &ctx) = nullptr;
};


struct game
{
    game();
    ~game() {
        for (const auto &ut : _unit_types)
            delete ut.second;
        for (auto *u : _units_top)
            delete u;
        for (auto *u : _units_bottom)
            delete u;
        for (auto *i : _items)
            delete i;
    }
    std::map<int, const unit_class *> _unit_types;
    std::vector<unit *> _units_top;
    std::vector<unit *> _units_bottom;
    std::vector<item *> _items;

    unit *create_unit(int unit_class_id) const {
        const unit_class *uc = _unit_types.at(unit_class_id);
        auto *u = new unit;
        u->_unit_class_id = unit_class_id;
        u->_hp = uc->_hp;
        u->_debug_instance_id = _next_id();
        return u;
    }

    static void rotate_units(std::vector<unit *> &units, ROTATE rotate) {
        if (units.size() < 2) return;
        if (rotate == LEFT) {
            for (int i = 0; i < units.size() - 1; ++i)
                std::swap(units[i], units[i + 1]);
        }
        if (rotate == RIGHT) {
            for (int i = units.size() - 2; i >= 0; --i)
                std::swap(units[i], units[i + 1]);
        }
    }

    item_use_context *_create_ctx(SIDE side) {
        struct _item_use_context : item_use_context
        {
            void damage_target(const int dmg) override {
                unit &t = *_targets[_target % _targets.size()];
                std::cout 
                    << "damage " 
                    << _game._unit_types.at(t._unit_class_id)->_debug_name
                    << " id=" << t._debug_instance_id
                    << " for " << dmg << "dmg\n";

                t._hp -= dmg;
            }
            void next_target(ROTATE rotate, int dist = 1) override {
                _target = (rotate == LEFT) ? (_target + dist) : (_target - dist);
            }
            void rotate_self(ROTATE rotate, int dist = 1) override {
                std::cout
                    << "rotate self "
                    << (rotate == LEFT ? "left" : "right")
                    << " for " << dist << "\n";
                while (dist--)
                    rotate_units(_selfs, rotate);
            }
            void swap_self(ROTATE rotate, int dist = 1) override {
                assert(false && "unimplemted!");
            }
            game &_game;
            std::vector<unit *> &_targets;
            std::vector<unit *> &_selfs;
            int _target = 0;
            int _self = 0;
            _item_use_context(game &_game, std::vector<unit *> &_targets, std::vector<unit *> &_selfs) :
                _game(_game), _targets(_targets), _selfs(_selfs) {}
        };
        if (side == TOP) {
            return new _item_use_context(*this, _units_bottom, _units_top);
        }
        if (side == BOTTOM) {
            return new _item_use_context(*this, _units_top, _units_bottom);
        }
        return nullptr;
    }

    void run_top() {
        if (_units_top.empty()) return;
        
        auto *ctx = _create_ctx(TOP);

        unit *u = *_units_top.begin();
        const unit_class *ut = _unit_types.at(u->_unit_class_id);
        auto *r = ut->_run;
        if (r)
            r(*ctx);

        delete ctx;
    }

    void run_bottom(int _self, int _item) {
        if (_units_bottom.empty()) return;
        
        auto *ctx = _create_ctx(BOTTOM);

        item *i = _items[_item];
        i->run(*ctx);

        delete ctx;
    }
};


struct unit_class_dwarf : unit_class
{
    inline static const int _id = _next_id();
    unit_class_dwarf() {
        _debug_name = "dwarf";
        _hp = 10;
    }
};


struct unit_class_gnoll : unit_class
{
    inline static const int _id = _next_id();
    static void _claw(item_use_context &ctx) {
        ctx.damage_target(1);
        ctx.rotate_self(RIGHT);
    }
    unit_class_gnoll() {
        _debug_name = "gnoll";
        _hp = 3;
        _run = _claw;
    }
};


struct item_sword : item
{
    void run(item_use_context &ctx) override {
        ctx.damage_target(5);
    }
};


struct item_axe : item
{
    void run(item_use_context &ctx) override {
        ctx.damage_target(3);
        ctx.next_target(RIGHT);
        ctx.damage_target(2);
        ctx.next_target(RIGHT);
        ctx.damage_target(1);
    }
};


game::game() {
    _unit_types = std::map<int, const unit_class *>{
        { unit_class_dwarf::_id, new unit_class_dwarf },
        { unit_class_gnoll::_id, new unit_class_gnoll },
    };


    _units_top = std::vector<unit *>{
        create_unit(unit_class_gnoll::_id),
        create_unit(unit_class_gnoll::_id),
        create_unit(unit_class_gnoll::_id),
    };

    _units_bottom = std::vector<unit *>{
        create_unit(unit_class_dwarf::_id),
    };

    _items = std::vector<item *>{
        new item_sword,
        new item_axe,
    };
}


int main() {

    game g;
    g.run_bottom(0, 1);
    g.run_top();

    return 0;
}