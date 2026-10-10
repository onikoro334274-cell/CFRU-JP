#include "defines.h"

/*
 * 通信の「遅れ」の判定をゆるくする（2026-10-08・docs/research/2026-10-08_link-lag.md）。
 * ユーザー「通信切断判定をゆるくしてほしい」→ 切れ方は「ゲームに つうしんエラー」。
 *
 * 素の LinkVSync（0800B0F8・pokefirered の src/link.c と同じ形）:
 *   - 親機: 1 コマのあいだにやり取りが 9 回終わっていないと、すぐ「遅れ（LAG_MASTER）」＝つうしんエラー
 *   - 子機: やり取りの割り込みが 10 コマ（約 0.17 秒）来ないと「遅れ（LAG_SLAVE）」
 *   ネット越し（mGBA の内蔵ネット通信・Celio）だと 0.2 秒くらいの遅れはふつうにあるので、ここで切れる。
 * → 親機・子機とも「割り込みが LINK_LAG_FRAMES コマ続けて来ない」ときだけ遅れにする。
 *   ★ 長くしすぎない: 送りの待ち行列（50 個・毎コマ 1 個積む）が約 50 コマであふれると、別のエラー（QUEUE_FULL）になる。
 *   合計値の食い違い・装置のエラー・あふれは今までどおり（無視すると両方の画面がずれて壊れる）。
 * 番地は日本語版の ROM を逆アセンブルして読んだ（gLink 0800B120・割り込みの来ないコマ 0800B164・SerialCB が 0800B1F4 で 0 に戻す）。
 */
#define LINK_LAG_FRAMES 45

#define gLinkRaw ((u8*) 0x03003FF0)        //struct Link（isMaster +0・state +1・playerCount +3・serialIntrCounter +13（s8）・link_field_F +15・hardwareError +16・lag +19）
#define sNoSerialIntrFrames (*(u8*) 0x03000E64)
#define LINK_STATE_HANDSHAKE 2
#define LINK_STATE_CONN_ESTABLISHED 4
#define LAG_MASTER 1
#define LAG_SLAVE 2
#define StartTransfer_ ((void (*)(void)) (0x0800B200 | 1))

void HackLink_VSync(void)
{
	u8* link = gLinkRaw;
	u8 state = link[1];

	if (link[0]) //親機
	{
		if (state == LINK_STATE_CONN_ESTABLISHED)
		{
			if ((s8) link[13] < 9)
			{
				if (link[16] == TRUE)
					StartTransfer_();
				else if (++sNoSerialIntrFrames > LINK_LAG_FRAMES) //素はここで すぐ LAG_MASTER
					link[19] = LAG_MASTER;
			}
			else if (link[19] != LAG_MASTER)
			{
				link[13] = 0;
				StartTransfer_();
			}
		}
		else if (state == LINK_STATE_HANDSHAKE)
		{
			StartTransfer_();
		}
	}
	else if (state == LINK_STATE_CONN_ESTABLISHED || state == LINK_STATE_HANDSHAKE)
	{
		if (++sNoSerialIntrFrames > LINK_LAG_FRAMES) //素は 10
		{
			if (state == LINK_STATE_CONN_ESTABLISHED)
				link[19] = LAG_SLAVE;
			if (state == LINK_STATE_HANDSHAKE)
			{
				link[3] = 0;
				link[15] = FALSE;
			}
		}
	}
}
