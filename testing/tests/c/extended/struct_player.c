#include <stdio.h>

typedef struct {
    int x;
    int y;
} Vec2D;

typedef struct {
    Vec2D position;
    int health;
} Player;

void move_player(Player *p, int dx, int dy) {
    p->position.x += dx;
    p->position.y += dy;
}

void hit_player(Player *p, int damage) {
    p->health -= damage;
    if (p->health < 0) p->health = 0;
}

int compute_score(Player p) {
    int dist = p.position.x * p.position.x + p.position.y * p.position.y;
    return dist + p.health;
}

int main(void) {
    Player p = {{0,0}, 100};
    move_player(&p, 3, 4);
    hit_player(&p, 30);
    int score = compute_score(p);
    printf("SCORE: %d\n", score);
    return 0;
}
