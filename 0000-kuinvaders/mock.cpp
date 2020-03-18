// Kuinvaders sample
//  Copyright Kuina-chan -- original version(Kuin Language)
//  Copyright Haruo Wakakusa -- SDL2 version

#include <SDL2/SDL_image.h>
#include <cmath>
#include <list>

using namespace std;

// * ウィンドウサイズについて
//    Kuin言語にはアスペクト比率を保持したままウィンドウの拡大・縮小を
//    行う機能がありますがSDLにはそれがないためウィンドウサイズは固定と
//    します。ただしオリジナルのプログラムの画面サイズが1600x900と
//    フルHDディスプレイを前提としているので、ウィンドウのほかに
//    レンダーターゲットを設けてオリジナルのプログラムをほぼ再現し、
//    かつ、小さいサイズのディスプレイにも対応できるようにします。

const SDL_Point RENDER_TARGET_SIZE { 1600, 900 };

//const SDL_Point WINDOW_SIZE { 1600, 900 }; // @ Hull HD display
const SDL_Point WINDOW_SIZE { 1120, 630 }; // @ HD display

SDL_Window* Window = NULL;
SDL_Renderer* Renderer = NULL;
SDL_Texture* RenderTarget = NULL;

//
// ヘルパー(マクロ・関数)
//

#define CLAMP(x, min1, max1) max(min1, min(x, max1))

inline float sqr(float x) {
	return x * x;
}

//
// QuitFlags変数 - ループの終了フラグ
//   (C++の例外処理をJavaのそれのように使わないためのもの)
//

struct tag_QuitFlags {
	bool app = false;
	bool titleScene = false;
	bool gameScene = false;
} QuitFlags;

//
// Textureクラス
//

class Texture {
	SDL_Texture* tex = NULL;
public:
	Texture(char *path) {
		SDL_Surface* s = IMG_Load(path);
		tex = SDL_CreateTextureFromSurface(Renderer, s);
		SDL_FreeSurface(s);
	}
	~Texture() {
		SDL_DestroyTexture(tex);
	}
	void draw(
		float dstX, float dstY, float srcX, float srcY, float w, float h
	) {
		SDL_Rect src = {
			roundl(srcX), roundl(srcY), roundl(w), round(h) };
		SDL_Rect dst = {
			roundl(dstX), roundl(dstY), src.w, src.h };
		SDL_RenderCopy(Renderer, tex, &src, &dst);
	}
};

//
// act関数 - 垂直同期でレンダーを行い、SDLイベントを処理する関数
//   (Kuinのwnd@act関数とdraw@render関数を合わせた関数)
//

void act() {
	SDL_Event e;
	SDL_SetRenderTarget(Renderer, NULL);
	SDL_RenderCopy(Renderer, RenderTarget, NULL, NULL);
	SDL_RenderPresent(Renderer);
	while (SDL_PollEvent(&e)) {
		if (e.type == SDL_QUIT) {
			QuitFlags.app = true;
			QuitFlags.titleScene = true;
			QuitFlags.gameScene = true;
		}
	}
	SDL_SetRenderTarget(Renderer, RenderTarget);
}

//
// titleScene関数 - タイトルシーンを実行
//

void titleScene() {
	Texture texTitle("res/title.jpg");
	QuitFlags.titleScene = false;
	while (!QuitFlags.titleScene) {
		act();
		if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A])
			QuitFlags.titleScene = true;
		texTitle.draw(0.0f, 0.0f, 0.0f, 0.0f, 1600.0f, 900.0f);
	}
}

//
// GameSceneクラス - ゲームシーンを実行
//  - GameSceneクラスは単一の大きなルーチンをいくつかのサブルーチンに
//    分割するためにクラス変数を共有します(Cのファイルスコープと同じ)
//

class GameScene {
private:
	static const int ENEMY_ROWS = 5;
	static const int ENEMY_COLUMNS = 6;

	Texture texBack { "res/back.jpg" };
	Texture texGame { "res/game.png" };
	int stage = 1;
	struct Beam {
		float x;
		float y;
	};

	// ステージ毎に初期化される変数
	float playerX, playerY;
	list<Beam> beams;
	int enemies[ENEMY_ROWS][ENEMY_COLUMNS]; // 敵の体力(初期値はすべて1)
	float enemyX, enemyY, enemyVeloX;
	float enemyTargetY; // 敵群のY方向の目的座標(下向きに移動するときに使用)
	int enemyLeft, enemyRight, enemyBottom, enemyNum;
	list<Beam> enemyBeams;
	enum tag_gameover {
		GAMEOVER_FLAG_PLAYING,
		GAMEOVER_FLAG_CLEARED,
		GAMEOVER_FLAG_DEAD
	} gameover;
	int frameCount; // Kuin@Cnt関数の代わり
	Uint8 prevShotButtonState; // Kuinvaderではキー押しっぱで連射されません

	void initForStartingStage() {
		playerX = 800.0f;
		playerY = 800.0f;
		beams.clear();
		for (int i = 0; i < ENEMY_ROWS; ++i) {
			for (int j = 0; j < ENEMY_COLUMNS; ++j) {
				enemies[i][j] = 1;
			}
		}
		enemyX = 200.0f / 2.0f;
		enemyY = 100.0f;
		enemyVeloX = 1.0 + (float)(stage) + 30.0 / (5.0 * 6.0);
		enemyTargetY = 0.0;
		enemyLeft = 0;
		enemyRight = ENEMY_COLUMNS - 1;
		enemyBottom = ENEMY_ROWS - 1;
		enemyNum = ENEMY_ROWS * ENEMY_COLUMNS;
		enemyBeams.clear();
		gameover = GAMEOVER_FLAG_PLAYING;
		frameCount = 0;
		prevShotButtonState = 0;
	}

	void drawBackgroundImage() {
		texBack.draw(
			0.0f, 0.0f, 0.0f, 0.0f,
			(float)RENDER_TARGET_SIZE.x,
			(float)RENDER_TARGET_SIZE.y);
	}

	void movePlayer() {
		const Uint8* keystate = SDL_GetKeyboardState(NULL);
		if (keystate[SDL_SCANCODE_LEFT]) playerX -= 12.0f;
		if (keystate[SDL_SCANCODE_RIGHT]) playerX += 12.0f;
		if (keystate[SDL_SCANCODE_UP]) playerY -= 12.0f;
		if (keystate[SDL_SCANCODE_DOWN]) playerY += 12.0f;
		playerX = CLAMP(playerX, 140.0f / 2.0f, 1600.0f - 140.0f / 2.0f);
		playerY = CLAMP(playerY, 700.0f, 900.0f);
	}

	void addPlayerBullet() {
		Beam beam { playerX, playerY - 220.0f / 2.0f };
		beams.push_front(beam);
	}

	void drawPlayer() {
		if (gameover != GAMEOVER_FLAG_DEAD) {
			texGame.draw(
				playerX - 140.0f / 2.0f, playerY - 220.0f / 2.0f,
				(float)(frameCount / 10 % 3) * 140.0f, 0.0f,
				140.0f, 220.0f);
		}
	}

	void moveEnemies() {
		if (gameover == GAMEOVER_FLAG_PLAYING) {
			if (enemyTargetY != 0.0f) {
				enemyY += 8.0; // 画面端で下向きに移動する
				if (enemyY >= enemyTargetY)
					enemyTargetY = 0.0f; // 目的座標まで到達したら目的座標をリセット
				if (enemyY + (float)(enemyBottom) * 45.0f > 700.0f) {
					gameover = GAMEOVER_FLAG_DEAD; // 敵が下ラインを超えたら死亡
				}
			} else {
				enemyX += enemyVeloX; // 左右への移動
				// 左端に到達
				if (enemyX <= 200.0f / 2.0f - (float)(enemyLeft) * 100.0f) {
					enemyX = 200.0f / 2.0f - (float)(enemyLeft) * 100.0f;
					enemyVeloX = 2.0f + (float)(stage) + 30.0f / (float)(enemyNum); // 残り敵数に応じた速度で右向きに折り返す
					enemyTargetY = enemyY + 60.0f; // 下向きに移動する
				}
				if (enemyX >= 1600.0f - 200.0f / 2.0f - (float)(enemyRight) * 100.0f) {
					enemyX = 1600.0f - 200.0f / 2.0f - (float)(enemyRight) * 100.0f;
					enemyVeloX = -(2.0f + (float)(stage) + 30.0f / (float)(enemyNum)); // 残り敵数に応じた速度で左向きに折り返す
					enemyTargetY = enemyY + 60.0f; // 下向きに移動する
				}
			}
		}
	}

	void addEnemyBullet() {
		for (int row = 0; row < ENEMY_ROWS; ++row) {
			for (int col = 0; col < ENEMY_COLUMNS; ++col) {
				if (enemies[row][col] == 0) continue;
				// 一定確率で弾を発射する
				if (rand() % (60 * enemyNum / min(stage, 30) + 1) == 0) {
					Beam beam;
					beam.x = enemyX + (float)(col) * 100.0f;
					beam.y = enemyY + (float)(row) * 45.0f;
					enemyBeams.push_front(beam);
				}
			}
		}
	}

	void drawEnemies() {
		for (int row = 0; row < ENEMY_ROWS; ++row) {
			for (int col = 0; col < ENEMY_COLUMNS; ++col) {
				if (enemies[row][col] == 0) continue;
				texGame.draw(
					enemyX - 200.0f / 2.0f + (float)(col) * 100.0f,
					enemyY - 120.0f / 2.0f + (float)(row) * 45.0f,
					(float)(frameCount / 10 % 3) * 200.0f,
					220.0f,
					200.0f,
					120.0f);
			}
		}
	}

	void movePlayerBullet() {
		list<Beam>::iterator i = beams.begin();
		while (i != beams.end()) {
			i->y -= 20.0f;
			if (i->y < -90.0f / 2.0f) {
				i = beams.erase(i); // 画面外に出たら削除
				continue;
			}
			// 敵との衝突判定
			for (int row = 0; row < ENEMY_ROWS; ++row) {
				for (int col = 0; col < ENEMY_COLUMNS; ++col) {
					// 距離が半径の合計以下の場合 -> 衝突
					if (enemies[row][col] > 0 && sqrt(sqr(enemyX + (float)(col) * 100.0f - i->x) + sqr(enemyY + (float)(row) * 45.0f - i->y)) <= 120.0f / 2.0f + 90.0f / 2.0f) {
						enemies[row][col] -= 1; // 敵のライフを減少
						if (enemies[row][col] == 0)
							enemyNum -= 1;
						i = beams.erase(i);
						goto GameScene_movePlayerBullet_loopBeams_end;
					}
				}
			}
	GameScene_movePlayerBullet_loopBeams_end:
			++i;
		}
	}

	void drawPlayerBullet() {
		list<Beam>::iterator beam = beams.begin();
		while (beam != beams.end()) {
			texGame.draw(beam->x - 90.0f / 2.0f, beam->y - 90.0f / 2.0f,
			420.0f, 0.0f,
			90.0f, 90.0f);
			++beam;
		}
	}

	void renewEnemyLeft() {
		while (1) {
			for (int row = 0; row < ENEMY_ROWS; ++row) {
				if (enemies[row][enemyLeft] > 0) return;
			}
			enemyLeft += 1;
		}
	}

	void renewEnemyRight() {
		while (1) {
			for (int row = 0; row < ENEMY_ROWS; ++row) {
				if (enemies[row][enemyRight] > 0) return;
			}
			enemyRight -= 1;
		}
	}

	void renewEnemyBottom() {
		while (1) {
			for (int col = 0; col < ENEMY_COLUMNS; ++col) {
				if (enemies[enemyBottom][col] > 0) return;
			}
			enemyBottom -= 1;
		}
	}

	void moveEnemyBullet() {
		list<Beam>::iterator b = enemyBeams.begin();
		while (b != enemyBeams.end()) {
			b->y += 10.0f;
			if (gameover == GAMEOVER_FLAG_PLAYING
				&& sqrt(sqr(b->x - playerX) + sqr(b->y - playerY)) <= 70.0f / 2.0f + 140.0f / 2.0f) {
				gameover = GAMEOVER_FLAG_DEAD;
			}
			if (b->y > 900.0f + 100.0f / 2.0f) {
				b = enemyBeams.erase(b);
				continue;
			}
			++b;
		}
	}

	void drawEnemyBullet() {
		for (
			list<Beam>::iterator b = enemyBeams.begin();
			b != enemyBeams.end();
			++b
		) {
			texGame.draw(
				b->x - 70.0f / 2.0f, b->y - 100.0f / 2.0f,
				420.0f, 90.0f,
				70.0f, 100.0f);
		}
	}

	void stageLoop() {
		initForStartingStage();

		// フレーム毎の描画
		while (!QuitFlags.gameScene) {
			act();
			drawBackgroundImage();
			movePlayer();

			// ショットボタンの解釈
			if (SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A]
				&& !prevShotButtonState
			) {
				if (gameover == GAMEOVER_FLAG_CLEARED) {
					stage += 1;
					return; // ステージクリアにつき次のステージへ
				} else if (gameover == GAMEOVER_FLAG_DEAD) {
					QuitFlags.gameScene = true;
					return; // 死亡につきタイトル画面へ
				}
				addPlayerBullet();
			}
			prevShotButtonState = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_A];

			drawPlayer();
			moveEnemies();
			addEnemyBullet();
			drawEnemies();
			movePlayerBullet();
			drawPlayerBullet();

			if (enemyNum == 0) {
				gameover = GAMEOVER_FLAG_CLEARED;
			} else {
				renewEnemyLeft();
				renewEnemyRight();
				renewEnemyBottom();
			}

			moveEnemyBullet();
			drawEnemyBullet();

			frameCount += 1;
		}
	}

public:
	void run() {
		QuitFlags.gameScene = false;
		while (!QuitFlags.gameScene) {
			stageLoop();
		}
	}

};

//
// main関数
//

int main(int argc, char* args[]) {
	SDL_Init(SDL_INIT_VIDEO);
	IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

	Window = SDL_CreateWindow(
		"Kuinvaders",
		SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		WINDOW_SIZE.x, WINDOW_SIZE.y,
		SDL_WINDOW_SHOWN);

	Renderer = SDL_CreateRenderer(
		Window,
		-1,
		SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

	RenderTarget = SDL_CreateTexture(
		Renderer,
		SDL_PIXELFORMAT_RGBA32,
		SDL_TEXTUREACCESS_TARGET,
		RENDER_TARGET_SIZE.x,
		RENDER_TARGET_SIZE.y);

	srand(0); // 常に同じランダムパターン。それとも引数に時間を入れますか？

	// Kuin言語に比べると超ダサいループ。どうしてこうなるのか考えてみよう！
	while (!QuitFlags.app) {
		titleScene();
		if (!QuitFlags.app) {
			GameScene g;
			g.run();
		}
	}

	IMG_Quit();
	SDL_Quit();
	return 0;
}

