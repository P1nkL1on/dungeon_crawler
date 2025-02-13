#include <iostream>
#include <memory>
#include <vector>


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


enum tag_type {
	DEAD,
	POISON,
};


struct counter
{
	tag_type _type = DEAD;
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
	int counter_cr(tag_type type) const {
		for (const auto &counter : _counters)
			if (counter->_type == type) return counter->_value;
		return 0;
	}
	int counter_bump(tag_type type, int incdec) {
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


struct unit
{
	int _hp = 100;
	int _power = 5;
	std::shared_ptr<const tags> _tags;

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
	int next_random(int min, int max) {
		assert(_rng, "can't be nullptr");
		auto *_tmp = _rng->clone();
		int num = _tmp->next(min, max);
		_rng = std::shared_ptr<const rng>(_tmp);
		return num;
	}
};


// impl
struct action_draw : action
{
	void run(game_state &game) const override {
		assert(!game.deck_cr()._cards.empty(), "deck is empty!");
		hand &_hand = game.hand_detach();
		hand &_deck = game.deck_detach();
		_hand._cards.push_back(_deck._cards.back());
		_deck._cards.pop_back();
	}
};


struct action_shuffle_deck : action
{
	void run(game_state &game) const override {
		std::vector<std::shared_ptr<const card>> cards = game.deck_cr()._cards;
		
		std::vector<std::shared_ptr<const card>> shuffled;
		shuffled.reserve(cards.size());

		while (!cards.empty()) {
			int idx = game.next_random(0, cards.size());
			shuffled.push_back(cards[idx]);
			cards.erase(cards.begin() + idx);
		}
		game.deck_detach()._cards = shuffled;
	}
};


struct action_strike : action
{
	team::side side = team::ALLY;

	void run(game_state &game) const override {
		game.team_detach(!side).unit_detach(team::LEADER)._hp -= 
			game.team_cr(side).unit_cr(team::LEADER)._power;
	}
};


struct action_clash : action
{
	team::side side = team::ALLY;

	void run(game_state &game) const override {
		
		action_strike strike;
		strike.side = side;
		strike.run(game);

		// TODO: check if backstriker is already dead

		action_strike strike_back;
		strike_back.side = !side;
		strike_back.run(game);
	}
};


struct action_play_top_card : action
{
	void run(game_state &game) const override {
		assert(!game.deck_cr()._cards.empty(), "deck is empty!");

		hand &deck = game.deck_detach();
		std::shared_ptr<const card> top_card = deck._cards.back();
		deck._cards.pop_back();

		top_card->_action->run(game);

		hand &discard = game.discard_detach();
		discard._cards.push_back(top_card);
	}
};


struct action_apply_poison : action
{
	void run(game_state &game) const override {
		game
			.team_detach(team::ENEMY)
			.unit_detach(team::LEADER)
			.tags_detach()
			.counter_bump(POISON, 10);
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


void draw_card() {
	game_state game;

	game._rng = std::make_shared<const random_counter>(42);

	auto c1 = std::make_shared<card>();
	auto c2 = std::make_shared<card>();
	auto c3 = std::make_shared<card>();
	
	auto deck = std::make_shared<hand>();
	deck->_cards = { c1, c2, c3 };

	auto phand = std::make_shared<hand>();
	game._deck = deck;
	game._hand = phand;

	game_state game2 = game;

	action_shuffle_deck shuffle;
	shuffle.run(game2);

	action_draw draw;
	draw.run(game);

	assert(game._hand != game2._hand, "hand was detached!");
	assert(game._deck != game2._deck, "deck was detached!");
	assert(game2._deck->_cards == (std::vector<std::shared_ptr<const card>>{ c3, c2 }), "");
	assert(game2._hand->_cards == std::vector<std::shared_ptr<const card>>{ c1 }, "");
}


int main() {
	std::cout << "Hello, world!\n"; 

	draw_card();

	return 0;
}