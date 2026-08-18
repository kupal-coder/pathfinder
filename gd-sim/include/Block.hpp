#pragma once
#include <Object.hpp>

struct Block : public Object {
    Block(Vec2D size, std::unordered_map<int, std::string>&& fields);
    std::unique_ptr<Object> clone() const override { return std::make_unique<Block>(*this); }
    void collide(Player&) const override;
};

struct BreakableBlock : public Block {
    using Block::Block;

    std::unique_ptr<Object> clone() const override { return std::make_unique<BreakableBlock>(*this); }
    void collide(Player&) const override;
    bool touching(Player const& p) const override;
};
