#include <iostream>
#include <memory>
#include <vector>
#include <list>


struct constraint_violated : public std::runtime_error
{
	constraint_violated(const std::string &what, const std::string &msg, const std::string &where, int line) : 
		std::runtime_error("\n\tfailed: " + what + "\n\tmeans:  " + msg +  "\n\tat:     " + where + "\n\tline:   " + std::to_string(line)) {}
};


#define assert(b, what) if (!static_cast<bool>((b))) throw constraint_violated(#b, ((what)), __func__, __LINE__)


struct game_state;


struct action
{
	virtual ~action() = default;
	virtual void run(game_state &) const = 0;
};


struct card
{
	std::shared_ptr<const action> _action;
};


enum class tag {
	DEAD,
	POISON,
	DAMAGE,
	POWER,
	INCAPACITATED,
};


struct counter
{
	tag _type = tag::DEAD;
	int _value = 0;
};


struct tags
{
	std::vector<std::shared_ptr<const counter>> _counters;

	int count() const {
		return _counters.size();
	}
	const counter &at(int idx) const {
		assert(0 <= idx && idx < count(), "bad index");
		return *_counters[idx];
	}
	int counter_cr(tag type) const {
		for (const auto &counter : _counters)
			if (counter->_type == type) return counter->_value;
		return 0;
	}
	int counter_bump(tag type, int incdec) {
		assert(incdec, "use counter_cr to access value without detach");
		for (auto &iter : _counters) {
			if (iter->_type != type) continue;
			auto *_tmp = new counter(*iter);
			_tmp->_value += incdec;
			iter = std::shared_ptr<const counter>(_tmp);
			return iter->_value;
		}
		auto *_tmp = new counter{};
		_tmp->_type = type; 
		_tmp->_value = incdec;
		_counters.emplace_back(_tmp);
		return incdec;
	}
};


enum class trigger {
	ATTACK,
	ALLY_ATTACK,
	ATTACKED,
	
	TAG_INC = 10000,
	HURT = TAG_INC + (int)tag::DAMAGE,
	DIED = TAG_INC + (int)tag::DEAD,
	
	ALLY_TAG_INC = 20000,
};

trigger operator+(trigger a, trigger b) {
	return trigger((int)a + (int)b);
}

trigger operator+(trigger a, tag b) {
	return trigger((int)a + (int)b);
}


struct ability
{
	trigger _trigger;
	std::shared_ptr<const action> _action;
};


struct abilities
{
	std::vector<std::shared_ptr<const ability>> _abilities;

	void add(trigger _trigger, action *_action) {
		_abilities.emplace_back(new ability{
			_trigger, std::shared_ptr<const action>(_action)});
	}

	std::list<std::shared_ptr<const action>> triggered(trigger _trigger) const {
		std::list<std::shared_ptr<const action>> _tmp;
		for (const auto &_ability : _abilities)
			if (_ability->_trigger == _trigger) _tmp.push_back(_ability->_action);
		return _tmp;
	}
};


struct unit
{
	int _hp = 100;
	int _power = 5;
	std::shared_ptr<const tags> _tags = std::make_shared<tags>();
	std::shared_ptr<const abilities> _abilities = std::make_shared<abilities>();

	const abilities &abilities_cr() const {
		assert(_abilities, "can't be nullptr");
		return *_abilities;
	}
	const tags &tags_cr() const {
		assert(_tags, "can't be nullptr");
		return *_tags;
	}
	tags &tags_detach() {
		assert(_tags, "can't be nullptr");
		auto *_tmp = new tags(*_tags);
		_tags = std::shared_ptr<const tags>(_tmp);
		return *_tmp;
	}
	abilities &abilities_detach() {
		assert(_abilities, "can't be nullptr");
		auto *_tmp = new abilities(*_abilities);
		_abilities = std::shared_ptr<const abilities>(_tmp);
		return *_tmp;
	}
};


struct team
{
	std::vector<std::shared_ptr<const unit>> _units;

	size_t count() const {
		return _units.size();
	}
	const unit &unit_cr(int idx) const {
		assert(count() != 0, "no units!");
		return *_units[idx % count()];
	}
	unit &unit_detach(int idx) {
		assert(count() != 0, "no units!");
		auto *_tmp = new unit(*_units[idx % count()]);
		_units[idx % count()] = std::shared_ptr<const unit>(_tmp);
		return *_tmp;
	}
	static constexpr int LEADER = 0;

	using side = unsigned char;
	static constexpr side ALLY = 0;
	static constexpr side ENEMY = 1;
};


struct rng
{
	virtual ~rng() = default;
	virtual void set_seed(int seed) = 0;
	virtual int next(int min, int max) = 0;
	virtual rng *clone() const = 0;
};


struct hand
{
	std::vector<std::shared_ptr<const card>> _cards;
};	


struct game_state
{
	virtual ~game_state() = default;
	virtual int unit_tag(team::side side, int idx, tag type) const = 0;
	virtual int bump_unit_tag(team::side side, int idx, tag type, int incdec) = 0;
	virtual int next_random(int min, int max) = 0;
	virtual void rotate(team::side side, int incdec) = 0;
	virtual void attack(team::side from, int idx, int opposite_idx) = 0;
	virtual void process_triggers() = 0;
	virtual team::side side() const = 0;
	virtual int idx() const = 0;

	bool is_lead() const { return idx() == team::LEADER; }
	bool is_alive() const { return unit_tag(side(), idx(), tag::DEAD) == 0; }
};


struct game_state_impl : game_state
{
	std::shared_ptr<const rng> _rng;
	std::shared_ptr<const hand> _deck;
	std::shared_ptr<const hand> _hand;
	std::shared_ptr<const hand> _discard;
	std::shared_ptr<const team> _allies;
	std::shared_ptr<const team> _enemies;

	hand &deck_detach() {
		assert(_deck, "can't be nullptr");
		auto *_tmp = new hand(*_deck);
		_deck = std::shared_ptr<const hand>(_tmp);
		return *_tmp;
	}
	hand &hand_detach() {
		assert(_hand, "can't be nullptr");
		auto *_tmp = new hand(*_hand);
		_hand = std::shared_ptr<const hand>(_tmp);
		return *_tmp;
	}
	hand &discard_detach() {
		assert(_discard, "can't be nullptr");
		auto *_tmp = new hand(*_discard);
		_discard = std::shared_ptr<const hand>(_tmp);
		return *_tmp;
	}
	team &allies_detach() {
		assert(_allies, "can't be nullptr");
		auto *_tmp = new team(*_allies);
		_allies = std::shared_ptr<const team>(_tmp);
		return *_tmp;
	}
	team &enemies_detach() {
		assert(_enemies, "can't be nullptr");
		auto *_tmp = new team(*_enemies);
		_enemies = std::shared_ptr<const team>(_tmp);
		return *_tmp;
	}
	team &team_detach(team::side side) {
		if (side == team::ALLY)
			return allies_detach();
		else if (side == team::ENEMY)
			return allies_detach();
		else
			assert(false, "unreachable");
	}
	const hand &deck_cr() const {
		assert(_deck, "can't be nullptr");
		return *_deck;
	}
	const hand &hand_cr() const {
		assert(_hand, "can't be nullptr");
		return *_hand;
	}
	const team &allies_cr() const {
		assert(_allies, "can't be nullptr");
		return *_allies;
	}
	const team &enemies_cr() const {
		assert(_enemies, "can't be nullptr");
		return *_enemies;
	}
	const team &team_cr(team::side side) const {
		if (side == team::ALLY)
			return allies_cr();
		else if (side == team::ENEMY)
			return enemies_cr();
		else
			assert(false, "unreachable");
	}

	// game_state interface impl
	int next_random(int min, int max) override {
		assert(_rng, "can't be nullptr");
		auto *_tmp = _rng->clone();
		int num = _tmp->next(min, max);
		_rng = std::shared_ptr<const rng>(_tmp);
		return num;
	}
	int unit_tag(team::side side, int idx, tag type) const override {
		return team_cr(side).unit_cr(idx).tags_cr().counter_cr(type);
	}
	int bump_unit_tag(team::side side, int idx, tag type, int incdec) override {
		assert(incdec, "use const methods to get a tag value");
		const int _tmp = team_detach(side).unit_detach(idx).tags_detach().counter_bump(type, incdec);

		if (incdec > 0) {
		}
		return _tmp;
	}
	void rotate(team::side side, int incdec) override {
		assert(incdec, "can't be 0");

		// TODO: actually rotate a team (with detach)

		for (plan &_plan : _queue)
			if (_plan._side == side) _plan._idx += incdec;
	}
	void attack(team::side from, int idx, int opposite_idx) override {
		queue_trigger(from, idx, trigger::ATTACK);
		queue_trigger(!from, opposite_idx, trigger::ATTACKED);
	}
	void process_triggers() override {
		while (!_queue.empty()) {
			_queue.front()._action->run(*this);
			_queue.pop_front();
		}
	}
	team::side side() const override {
		assert(!_queue.empty(), "this action can't run without a trigger queue");
		return _queue.front()._side;
	}
	int idx() const override {
		assert(!_queue.empty(), "this action can't run without a trigger queue");
		return _queue.front()._idx;
	}
	void queue_trigger(team::side side, int idx, trigger _trigger) {
		const auto _actions = team_cr(side).unit_cr(idx).abilities_cr().triggered(_trigger);
		for (const auto &_action : _actions)
			_queue.push_back(plan{_action, side, idx});
	}
	struct plan {
		std::shared_ptr<const action> _action;
		team::side _side;
		int _idx;
	};
	std::list<plan> _queue;
};


// impl
struct action_draw : action
{
	void run(game_state &game) const override {
		// assert(!game.deck_cr()._cards.empty(), "deck is empty!");
		// hand &_hand = game.hand_detach();
		// hand &_deck = game.deck_detach();
		// _hand._cards.push_back(_deck._cards.back());
		// _deck._cards.pop_back();
	}
};


struct action_shuffle_deck : action
{
	void run(game_state &game) const override {
		// std::vector<std::shared_ptr<const card>> cards = game.deck_cr()._cards;
		
		// std::vector<std::shared_ptr<const card>> shuffled;
		// shuffled.reserve(cards.size());

		// while (!cards.empty()) {
		// 	int idx = game.next_random(0, cards.size());
		// 	shuffled.push_back(cards[idx]);
		// 	cards.erase(cards.begin() + idx);
		// }
		// game.deck_detach()._cards = shuffled;
	}
};


struct action_strike : action
{
	team::side side = team::ALLY;

	void run(game_state &game) const override {
		if (!game.is_alive())
			return;
		if (game.unit_tag(game.side(), game.idx(), tag::INCAPACITATED))
			return; // miss
		const int dmg = game.unit_tag(side, team::LEADER, tag::POWER);
		if (!dmg)
			return;
		game.attack(game.side(), game.idx(), team::LEADER);
		game.bump_unit_tag(!side, team::LEADER, tag::DAMAGE, dmg);
		game.process_triggers();
	}
};


// strong strike: x2 dmg, miss if enemy has unplayed cards
// reckless: strike, but after it you are incapacitated
// range: target any
// support: any your attacks enemy leader
// clash: strike + strike back
// shock: deal damage (no strike)
// blast: mass shock
// rotate random
// rotate left
// rotate right
// rotate any
// leader swap
// boss swap
// boss retreat
// +2 str to TE
// +1 str til END
// +2 str til END any
// heal
// guard
// guard any
// 


struct action_play_top_card : action
{
	void run(game_state &game) const override {
		// assert(!game.deck_cr()._cards.empty(), "deck is empty!");

		// hand &deck = game.deck_detach();
		// std::shared_ptr<const card> top_card = deck._cards.back();
		// deck._cards.pop_back();

		// top_card->_action->run(game);

		// hand &discard = game.discard_detach();
		// discard._cards.push_back(top_card);
	}
};


struct action_apply_poison : action
{
	void run(game_state &game) const override {
		game.bump_unit_tag(team::ENEMY, team::LEADER, tag::POISON, 10);
	}
};


struct random_counter : rng
{
	explicit random_counter(int seed) { set_seed(seed); }
	void set_seed(int seed) override {
		_counter = seed;
	}
	int next(int min, int max) override {
		assert(min < max, "invalid range!");
		if (min == max) return min;
		return min + (_counter++) % (max - min + 1);
	}
	random_counter *clone() const override {
		return new random_counter{ _counter };
	}
private:
	int _counter = 0;
};


std::shared_ptr<const unit> create_spiky() {

	struct dmg_back : action
	{
		void run(game_state &game) const override {
			game.bump_unit_tag(team::ENEMY, team::LEADER, tag::DAMAGE, 3);
		}
	};

	auto *_tmp = new unit;
	_tmp->_hp = 12;
	_tmp->_power = 4;
	_tmp->abilities_detach().add(trigger::HURT, new dmg_back);
	return std::shared_ptr<const unit>(_tmp);
}


void draw_card() {
	game_state_impl game;

	game._rng = std::make_shared<const random_counter>(42);

	auto c1 = std::make_shared<card>();
	auto c2 = std::make_shared<card>();
	auto c3 = std::make_shared<card>();
	
	auto deck = std::make_shared<hand>();
	deck->_cards = { c1, c2, c3 };

	auto phand = std::make_shared<hand>();
	game._deck = deck;
	game._hand = phand;

	game_state_impl game2 = game;

	action_shuffle_deck shuffle;
	shuffle.run(game2);

	action_draw draw;
	draw.run(game);

	// assert(game._hand != game2._hand, "hand was detached!");
	// assert(game._deck != game2._deck, "deck was detached!");
	// assert(game2._deck->_cards == (std::vector<std::shared_ptr<const card>>{ c3, c2 }), "");
	// assert(game2._hand->_cards == std::vector<std::shared_ptr<const card>>{ c1 }, "");
}


int main() {
	std::cout << "Hello, world!\n"; 

	draw_card();

	return 0;
}