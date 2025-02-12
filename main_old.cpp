#include <map>
#include <vector>
#include <iostream>
#include <cassert>
#include <algorithm>


int _next_id() 
{ 
    static int _counter = 0;
    return _counter++; 
}


enum ROTATE { LEFT, RIGHT };
enum SIDE { BOTTOM, TOP };
enum ROOMTYPE { EMPTY, ENEMY, SHOP, };


struct context 
{
    virtual ~context() = default;
    virtual bool next_target(ROTATE rotate, int dist = 1) = 0;
    virtual void rotate_self(ROTATE rotate, int dist = 1) = 0;
    virtual void rotate_target(ROTATE rotate, int dist = 1) = 0;
    virtual void swap_self(ROTATE rotate, int dist = 1) = 0;
    virtual void spawn_target(int unit_class_id) = 0;
    virtual int bump_tag_self(int tag, int change = 0) = 0;
    virtual int bump_tag_target(int tag, int change = 0) = 0;
};


struct item
{
    virtual ~item() = default;
    virtual void run(context &) {}
    virtual void turn_end(context &) {}
};


struct tag_type
{

};


struct tag_power : tag_type
{
    inline static const int _id = _next_id();
};


struct tag
{
    int _value = 0;
};


struct unit
{
    int _unit_class_id = -1;
    int _debug_instance_id = -1;
    std::map<int, tag> _tags;
};


struct unit_class
{
    std::string _debug_name = "";
    void (*_run)(context &ctx) = nullptr;
    void (*_spawn)(context &ctx) = nullptr;
};


struct room
{
    std::string _debug_name = "";
    ROOMTYPE _type = EMPTY;
    void (*_enter)(context &ctx) = nullptr; 
};


struct player
{
    virtual ~player() = default;
    virtual void use_item(int &_unit, int &_item) = 0;
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
        for (auto *i : _item_hand)
            delete i;
        for (auto *i : _item_deck)
            delete i;
        for (auto *i : _item_discard)
            delete i;
    }
    std::map<int, const unit_class *> _unit_types;
    std::vector<unit *> _units_top;
    std::vector<unit *> _units_bottom;
    std::vector<item *> _item_deck;
    std::vector<item *> _item_hand = { nullptr, nullptr, nullptr };
    std::vector<item *> _item_discard;
    room *_room = nullptr;
    int _turn = 0;

    const unit &_cr_unit(int idx, SIDE side) const {
        return side == TOP 
                ? *_units_top[idx % _units_top.size()]
                : *_units_bottom[idx % _units_bottom.size()];
    }
    unit &_r_unit(int idx, SIDE side) {
        return side == TOP 
                ? *_units_top[idx % _units_top.size()]
                : *_units_bottom[idx % _units_bottom.size()];
    }
    const unit_class &_cr_unit_class(const unit &cr) const {
        return *_unit_types.at(cr._unit_class_id);
    }

    unit *create_unit(int unit_class_id) const {
        auto *u = new unit;
        u->_unit_class_id = unit_class_id;
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

    context *_create_ctx(SIDE side, int idx = 0) {
        struct _context : context
        {
            bool next_target(ROTATE rotate, int dist = 1) override {
                std::cout << "  next target " << (rotate == LEFT ? "left" : "right") << ' ' << dist << "\n";
                _target = (rotate == LEFT) ? (_target + dist) : (_target - dist);
                return true;
            }
            void rotate_self(ROTATE rotate, int dist = 1) override {
                std::cout << "  rotate self " << (rotate == LEFT ? "left" : "right") << ' ' << dist << "\n";
                while (dist--)
                    rotate_units(_selfs, rotate);
            }
            void rotate_target(ROTATE rotate, int dist = 1) override {
                std::cout << "  rotate target " << (rotate == LEFT ? "left" : "right") << ' ' << dist << "\n";
                while (dist--)
                    rotate_units(_targets, rotate);
            }
            void swap_self(ROTATE rotate, int dist = 1) override {
                assert(false && "unimplemted!");
            }
            void spawn_target(int unit_class_id) override {
                std::cout << "  spawn target " << _game._unit_types.at(unit_class_id)->_debug_name << "\n";
                _targets.push_back(_game.create_unit(unit_class_id));
            }
            int bump_tag_self(int tag, int change) override {
                int &v = _game._r_unit(_self, _side)._tags[tag]._value;
                v += change;
                if (change)
                    std::cout << "  self tag " << tag << ' ' << (change > 0 ? "+" : "") << change << " -> " << v << '\n';
                return v;
            }
            int bump_tag_target(int tag, int change) override {
                int &v = target()._tags[tag]._value;
                v += change;
                if (change)
                    std::cout << "  target tag " << tag << ' ' << (change > 0 ? "+" : "") << change << " -> " << v << '\n';
                return v;
            }
            unit &target() { return *_targets[_target % _targets.size()]; }
            unit &self() { return *_selfs[_self % _selfs.size()]; }
            game &_game;
            std::vector<unit *> &_targets;
            std::vector<unit *> &_selfs;
            int _target = 0;
            int _self = 0;
            SIDE _side = BOTTOM;
            _context(game &_game, std::vector<unit *> &_targets, std::vector<unit *> &_selfs, int _target, int _self) :
                _game(_game), _targets(_targets), _selfs(_selfs), _target(_target), _self(_self) {}
        };
        if (side == TOP) {
            return new _context(*this, _units_bottom, _units_top, 0, idx);
        }
        if (side == BOTTOM) {
            return new _context(*this, _units_top, _units_bottom, idx, 0);
        }
        return nullptr;
    }

    //! return whether there are units to continue turns
    bool run_top() {
        if (_units_top.empty()) return false;
        
        auto *ctx = _create_ctx(TOP);

        unit *u = *_units_top.begin();
        const unit_class *ut = _unit_types.at(u->_unit_class_id);
        auto *r = ut->_run;
        if (r)
            r(*ctx);

        delete ctx;
        return true; // TODO: return cleanup();
    }


    //! return whether there are units to continue turns
    bool run_bottom(int _self, int _item) {
        if (_units_bottom.empty()) return false;
        
        auto *ctx = _create_ctx(BOTTOM);

        item *i = _item_hand[_item];
        i->run(*ctx);

        delete ctx;
        return true; // TODO: return cleanup();
    }

    void run(player &_player) {
        _turn = 0;
        auto *ctx = _create_ctx(BOTTOM);
        _room->_enter(*ctx);
        delete ctx;

        for (int i = 0; i < _units_top.size(); ++i) {
            ctx = _create_ctx(TOP, i);
            const unit_class *ut = _unit_types.at(_units_top[i]->_unit_class_id);
            ut->_spawn(*ctx);
            delete ctx;
        }

        for (;;) {
            _turn = _turn + 1;

            // replace empty hand slots
            for (item *&i : _item_hand) {
                if (i) continue;
                if (_item_deck.empty()) continue;
                i = _item_deck.back();
                _item_deck.pop_back();
            }

            int _self, _item;
            _player.use_item(_self, _item);
            
            std::cout << "PC acts!\n";
            bool player_lost = !run_bottom(_self, _item);
            std::cout << "PC lost? " << (player_lost ? "yes" : "no") << "\n";
            if (player_lost)
                break;

            std::cout << "room acts!\n";
            bool room_cleaned = !run_top();
            std::cout << "room cleaned? " << (room_cleaned ? "yes" : "no") << "\n";
            if (room_cleaned)
                break;
        } 
    }
};


struct unit_class_dwarf : unit_class
{
    inline static const int _id = _next_id();
    static void spawn(context &ctx) {
        ctx.bump_tag_self(tag_power::_id, 10);
    }
    unit_class_dwarf() {
        _debug_name = "dwarf";
        _spawn = spawn;
    }
};


struct unit_class_gnoll : unit_class
{
    inline static const int _id = _next_id();
    static void spawn(context &ctx) {
        ctx.bump_tag_self(tag_power::_id, 3);
    }
    static void claw(context &ctx) {
        ctx.bump_tag_self(tag_power::_id, 1);
        ctx.bump_tag_target(tag_power::_id, -1);
        ctx.rotate_self(RIGHT);
    }
    unit_class_gnoll() {
        _debug_name = "gnoll";
        _run = claw;
        _spawn = spawn;
    }
};


struct unit_class_shop_keeper : unit_class
{
    inline static const int _id = _next_id();
    static void spawn(context &ctx) {
        ctx.bump_tag_self(tag_power::_id, 100);
    }
    unit_class_shop_keeper() {
        _debug_name = "shop_keeper";
        _run = nullptr;
        _spawn = spawn;
    }
};


struct room_enc_gnoll : room
{
    static void enter(context &ctx) {
        ctx.spawn_target(unit_class_gnoll::_id);
        ctx.spawn_target(unit_class_gnoll::_id);
        ctx.spawn_target(unit_class_gnoll::_id);
    }
    room_enc_gnoll() {
        _debug_name = "enc_gnoll";
        _type = ENEMY;
        _enter = enter;
    }
};


struct room_shop : room
{
    static void enter(context &ctx) {
        ctx.spawn_target(unit_class_shop_keeper::_id);
    }
    room_shop() {
        _debug_name = "shop";
        _type = SHOP;
        _enter = enter;
    }
};


struct item_sword : item
{
    void run(context &ctx) override {
        ctx.bump_tag_target(tag_power::_id, -5);
    }
};


struct item_axe : item
{
    void run(context &ctx) override {
        ctx.bump_tag_target(tag_power::_id, -3);
        if (ctx.next_target(RIGHT))
            ctx.bump_tag_target(tag_power::_id, -2);
        if (ctx.next_target(RIGHT))
            ctx.bump_tag_target(tag_power::_id, -1);
    }
};


struct item_morningstar : item
{
    void run(context &ctx) override {
        ctx.bump_tag_target(tag_power::_id, -4);
        ctx.rotate_target(RIGHT, 1);
    }
};


game::game() {
    _unit_types = std::map<int, const unit_class *>{
        { unit_class_dwarf::_id, new unit_class_dwarf },
        { unit_class_gnoll::_id, new unit_class_gnoll },
        { unit_class_shop_keeper::_id, new unit_class_shop_keeper },
    };

    _units_bottom = std::vector<unit *>{
        create_unit(unit_class_dwarf::_id),
    };

    _item_deck = std::vector<item *>{
        new item_sword,
        new item_axe,
        new item_morningstar,
    };

    _room = new room_enc_gnoll;
}


struct player_cin : player
{
    void use_item(int &_unit, int &_item) override {
        std::cout << "waits <unit_idx> <item_idx>...\n";
        std::cin >> _unit >> _item;
    }
};


int main() {

    player_cin p;
    game g;
    g.run(p);
    // g.run_bottom(0, 1);
    // g.run_top();

    return 0;
}