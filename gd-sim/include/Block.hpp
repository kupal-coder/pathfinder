#pragma once
#include <Object.hpp>

struct Block : public Object {
    Block(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player&) const override;
};

struct BreakableBlock : public Block {
    using Block::Block;

    void collide(Player&) const override;
    bool touching(Player const& p) const override;
};

enum class SpecialBlockType {
    S, // Stop Dash (1813)
    J, // Prevent Jump Buffer (1814)
    H, // Head Collision Safety (1815)
    D  // Wave Slide (1816)
};

struct SpecialBlock : public Object {
    SpecialBlockType type;

    SpecialBlock(Vec2D size, std::unordered_map<int, std::string>&& fields);
    void collide(Player&) const override;
};
