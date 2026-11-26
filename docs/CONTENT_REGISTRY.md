# 현재 콘텐츠 레지스트리

공개 API에서 추출한73건물·35동물·15자원·24연구·85교환입니다. 정의 수치의 정본은 core/src/buildings/catalog.c, core/src/ecology/species.c, core/src/buildings/progression.c, core/src/economy/wandering_merchant.c이며 리소스 경로 정본은 game/assets/assets_manifest.json입니다.

## 건물

회전은0정면·1측면·2후면·3반대측면입니다. 지원되는 방향은 각 flags와 UI 제한을 따릅니다. 영주관은6단계 전용 시트를 사용합니다.

|ID|키|이름|기본 점유|해금|일자리|리소스|
|---:|---|---|---|---|---:|---|
|0|keep|영주관|4×4|개척지|0|buildings/keep_{0..3}.png + buildings/manor_{0..5}.png|
|1|house|주택|3×3|마을|0|buildings/house_{0..3}.png|
|2|farm|농장|3×3|마을|3|buildings/farm_{0..3}.png|
|3|gatherer|채집장|2×2|개척지|2|buildings/gatherer_{0..3}.png|
|4|lumber|벌목장|2×2|촌락|3|buildings/lumber_{0..3}.png|
|5|quarry|채석장|2×2|촌락|3|buildings/quarry_{0..3}.png|
|6|mana_mine|심층 광산|2×2|도시|4|buildings/mana_mine_{0..3}.png|
|7|warehouse|창고|3×2|마을|0|buildings/warehouse_{0..3}.png|
|8|market|시장|4×2|마을|2|buildings/market_{0..3}.png|
|9|barracks|병영|3×2|촌락|3|buildings/barracks_{0..3}.png|
|10|tower|감시탑|1×1|마을|2|buildings/tower_{0..3}.png|
|11|wall|성벽|1×1|마을|0|buildings/wall_{0..3}.png|
|12|gate|성문|1×1|마을|0|buildings/gate_{0..3}.png|
|13|healer|치료소|2×2|촌락|1|buildings/healer_{0..3}.png|
|14|mage_tower|마탑|3×3|성채|2|buildings/mage_tower_{0..3}.png|
|15|road|흙길|1×1|개척지|0|buildings/road_{0..3}.png|
|16|shack|막사|2×2|개척지|0|buildings/shack_{0..3}.png|
|17|cottage|오두막|2×3|촌락|0|buildings/cottage_{0..3}.png|
|18|mansion|저택|5×3|도시|0|buildings/mansion_{0..3}.png|
|19|garden|텃밭|2×2|개척지|1|buildings/garden_{0..3}.png|
|20|cropland|경작지|3×2|촌락|2|buildings/cropland_{0..3}.png|
|21|orchard|과수원|4×2|마을|2|buildings/orchard_{0..3}.png|
|22|greenhouse|온실|3×3|도시|3|buildings/greenhouse_{0..3}.png|
|23|irrigated_farm|관개농장|6×4|성채|5|buildings/irrigated_farm_{0..3}.png|
|24|windmill|풍차|3×3|왕성|0|buildings/windmill_{0..3}.png|
|25|ranch|축사|4×3|마을|3|buildings/ranch_{0..3}.png|
|26|pasture|방목장|6×5|성채|5|buildings/pasture_{0..3}.png|
|27|logging_site|벌목지|2×2|개척지|2|buildings/logging_site_{0..3}.png|
|28|forestry|조림지|3×3|도시|4|buildings/forestry_{0..3}.png|
|29|mine|광산|2×2|마을|4|buildings/mine_{0..3}.png|
|30|fishing_spot|낚시터|2×2|개척지|2|buildings/fishing_spot_{0..3}.png|
|31|port|항구|3×3|도시|4|buildings/port_{0..3}.png|
|32|palisade|목책|1×1|개척지|0|buildings/palisade_{0..3}.png|
|33|barrier|방벽|1×1|촌락|0|buildings/barrier_{0..3}.png|
|34|iron_wall|철벽|1×1|성채|0|buildings/iron_wall_{0..3}.png|
|35|palisade_gate|목책문|1×1|개척지|0|buildings/palisade_gate_{0..3}.png|
|36|stone_gate|석조문|1×1|촌락|0|buildings/stone_gate_{0..3}.png|
|37|iron_gate|철갑문|1×1|성채|0|buildings/iron_gate_{0..3}.png|
|38|outpost|초소|1×1|개척지|2|buildings/outpost_{0..3}.png|
|39|watchtower|망루|1×1|촌락|2|buildings/watchtower_{0..3}.png|
|40|crossbow_tower|쇠뇌탑|2×2|도시|2|buildings/crossbow_tower_{0..3}.png|
|41|cannon_tower|포탑|2×2|성채|2|buildings/cannon_tower_{0..3}.png|
|42|arcane_tower|마도 포탑|3×3|왕성|2|buildings/arcane_tower_{0..3}.png|
|43|stall|가판대|2×2|촌락|1|buildings/stall_{0..3}.png|
|44|trading_post|교역소|5×3|왕성|4|buildings/trading_post_{0..3}.png|
|45|granary|저장고|2×2|촌락|0|buildings/granary_{0..3}.png|
|46|logistics|물류창고|4×3|도시|0|buildings/logistics_{0..3}.png|
|47|sawmill|제재소|2×2|촌락|1|buildings/sawmill_{0..3}.png|
|48|kiln|가마|2×2|촌락|1|buildings/kiln_{0..3}.png|
|49|smelter|제련소|2×2|마을|2|buildings/smelter_{0..3}.png|
|50|well|우물|1×1|개척지|0|buildings/well_{0..3}.png|
|51|bathhouse|목욕탕|3×2|도시|1|buildings/bathhouse_{0..3}.png|
|52|hospital|병원|3×3|성채|3|buildings/hospital_{0..3}.png|
|53|tavern|주점|2×2|마을|1|buildings/tavern_{0..3}.png|
|54|school|학교|4×2|도시|2|buildings/school_{0..3}.png|
|55|library|도서관|3×3|도시|2|buildings/library_{0..3}.png|
|56|theatre|극장|3×3|성채|3|buildings/theatre_{0..3}.png|
|57|church|교회|3×3|성채|3|buildings/church_{0..3}.png|
|58|academy|학술원|4×4|왕성|4|buildings/academy_{0..3}.png|
|59|magic_workshop|마법 공방|2×2|도시|1|buildings/magic_workshop_{0..3}.png|
|60|grand_mage_tower|대마탑|4×4|왕성|5|buildings/grand_mage_tower_{0..3}.png|
|61|hunter_camp|야영지|2×2|촌락|2|buildings/hunter_camp_{0..3}.png|
|62|hunting_lodge|수렵기지|3×3|마을|4|buildings/hunting_lodge_{0..3}.png|
|63|stone_road|자갈길|1×1|마을|0|buildings/stone_road_{0..3}.png|
|64|bridge|교량|1×1|도시|0|buildings/bridge_{0..3}.png|
|65|garrison|군영|5×3|성채|6|buildings/garrison_{0..3}.png|
|66|clay_pit|채토장|2×2|촌락|2|buildings/clay_pit_{0..3}.png|
|67|water_tower|급수탑|2×2|도시|0|buildings/water_tower_{0..3}.png|
|68|observatory|천문대|3×3|왕성|4|buildings/observatory_{0..3}.png|
|69|art_museum|미술관|4×4|왕성|2|buildings/art_museum_{0..3}.png|
|70|lighthouse|등대|2×2|성채|0|buildings/lighthouse_{0..3}.png|
|71|clinic|의원|3×2|마을|2|buildings/clinic_{0..3}.png|
|72|paved_road|돌길|1×1|성채|0|buildings/paved_road_{0..3}.png|

## 동물

같은 종 ID가 animals/walk.png·attack.png·swim.png·young.png의 행을 가리킵니다. 각 이동·수영·새끼는4프레임입니다. 해당 행동이 없는 종은 행동 조건으로 제외합니다. 전용 피격·사망 리소스는 없습니다.

|ID|이름|HP|공격|속도×1000|수영×1000|가축|
|---:|---|---:|---:|---:|---:|---:|
|0|토끼|10|0|1520|0|0|
|1|사슴|42|0|1480|0|0|
|2|산양|34|3|1260|0|0|
|3|오리|10|0|780|1100|0|
|4|여우|21|3|1380|0|0|
|5|낙타|128|0|1120|0|0|
|6|물개|50|0|480|1800|0|
|7|소|65|0|700|0|1|
|8|양|28|0|900|0|1|
|9|돼지|36|0|850|0|1|
|10|닭|9|0|1000|0|1|
|11|늑대|56|7|1420|0|0|
|12|곰|118|15|1160|0|0|
|13|악어|150|18|720|1250|0|
|14|버팔로|172|14|1230|0|0|
|15|뱀|14|4|880|0|0|
|16|사자|105|16|1480|0|0|
|17|코뿔소|200|21|1250|0|0|
|18|개구리|5|0|720|1000|0|
|19|하이에나|44|8|1340|0|0|
|20|스라소니|30|6|1440|0|0|
|21|펭귄|16|0|630|1500|0|
|22|타조|60|5|1850|0|0|
|23|꿩|10|0|1220|0|0|
|24|유니콘|170|18|1450|0|0|
|25|코카트리스|150|16|1100|0|0|
|26|그리핀|260|28|1550|0|0|
|27|펜리르|340|34|1400|0|0|
|28|히드라|420|30|700|0|0|
|29|샐러맨더|190|20|1050|0|0|
|30|얼룩말|92|0|1600|0|0|
|31|카피바라|38|0|950|1250|0|
|32|염소|42|0|1100|0|1|
|33|알파카|64|0|1050|0|1|
|34|칠면조|20|0|950|0|1|

## 자원

표시는 아래 순서입니다. 저장/API 인덱스 순서와 별개이며 ID를 바꾸지 않습니다.

|표시 순서|ID|키|이름|아이콘|
|---:|---:|---|---|---|
|1|0|gold|금화|ui/resources/gold.png|
|2|1|mana|마정석|ui/resources/mana.png|
|3|2|food|식량|ui/resources/food.png|
|4|6|logs|원목|ui/resources/logs.png|
|5|3|wood|목재|ui/resources/wood.png|
|6|4|stone|돌|ui/resources/stone.png|
|7|5|herbs|약초|ui/resources/herbs.png|
|8|14|leather|가죽|ui/resources/leather.png|
|9|7|clay|점토|ui/resources/clay.png|
|10|8|bricks|벽돌|ui/resources/bricks.png|
|11|10|coal|석탄|ui/resources/coal.png|
|12|9|iron_ore|철광석|ui/resources/iron_ore.png|
|13|11|iron|철괴|ui/resources/iron.png|
|14|12|gold_ore|금광석|ui/resources/gold_ore.png|
|15|13|gold_bar|금괴|ui/resources/gold_bar.png|

## 연구

|ID|키|해금|금화|마정석|점수|
|---:|---|---|---:|---:|---:|
|0|rotation|도시|70|0|80|
|1|tools|도시|90|0|100|
|2|water_survey|도시|100|5|120|
|3|logistics|성채|130|10|150|
|4|seeds|성채|180|15|190|
|5|smelting|성채|190|18|210|
|6|storage|왕성|250|25|260|
|7|royal_trade|왕성|320|30|300|
|8|scouting|도시|70|0|80|
|9|longbows|도시|90|0|100|
|10|gatecraft|도시|100|5|120|
|11|armor|성채|150|10|160|
|12|gunpowder|성채|190|15|200|
|13|fortification|성채|200|20|220|
|14|tactics|왕성|260|25|260|
|15|royal_defense|왕성|340|30|310|
|16|mana_survey|성채|100|10|120|
|17|abundance|성채|140|15|150|
|18|healing_light|성채|160|20|170|
|19|embers|성채|180|25|190|
|20|mana_control|성채|220|30|240|
|21|ward|왕성|260|35|280|
|22|alchemy|왕성|300|45|320|
|23|grand_magic|왕성|400|60|400|

## 떠돌이 상인 교환

교환 ID와 nonce로 중복 실행을 거부합니다. 혜택 bp는 모두 양수입니다.

|ID|해금|지불|수령|최대 횟수|가중치|혜택bp|
|---:|---|---|---|---:|---:|---:|
|1|촌락|원목 30|마정석 2|2|12|5000|
|2|촌락|약초 12|마정석 1|3|12|2500|
|3|촌락|식량 50|마정석 2|2|12|8000|
|4|촌락|돌 22|마정석 2|2|12|3636|
|5|촌락|점토 24|마정석 2|2|12|8750|
|6|마을|가죽 18|마정석 3|2|10|2500|
|7|마을|목재 18|마정석 2|2|10|2500|
|8|마을|벽돌 14|마정석 2|2|10|2857|
|9|도시|철광석 18|마정석 2|3|10|2500|
|10|도시|석탄 20|마정석 2|2|10|5000|
|11|도시|철괴 4|마정석 2|2|10|8750|
|12|성채|금광석 4|마정석 2|2|8|2500|
|13|왕성|금괴 2|마정석 2|2|8|1250|
|14|촌락|원목 20|식량 45|3|12|12500|
|15|촌락|식량 40|목재 12|3|12|14000|
|16|촌락|약초 10|가죽 7|3|12|18000|
|17|촌락|점토 16|벽돌 11|3|12|27812|
|18|마을|돌 25|철광석 13|2|10|5600|
|19|도시|목재 20|석탄 18|2|9|5750|
|20|도시|가죽 12|약초 13|3|9|833|
|21|성채|석탄 14|철괴 4|2|8|10952|
|22|촌락|원목 14|돌 12|3|12|15714|
|23|촌락|약초 8|식량 25|3|12|10833|
|24|촌락|돌 18|원목 18|3|12|6666|
|25|마을|점토 20|석탄 10|2|10|7500|
|26|마을|식량 36|가죽 8|3|10|16666|
|27|도시|철광석 15|벽돌 12|2|9|12000|
|28|성채|철괴 6|금광석 4|2|8|6666|
|29|왕성|금광석 5|금괴 3|2|8|10000|
|30|촌락|원목 18|돌 19|2|11|21666|
|31|촌락|돌 18|원목 25|2|11|13148|
|32|촌락|식량 30|원목 15|3|11|15000|
|33|촌락|원목 20|약초 19|2|11|18500|
|34|촌락|약초 12|식량 45|2|11|15000|
|35|촌락|점토 20|돌 20|2|11|20000|
|36|촌락|돌 12|점토 17|3|11|13611|
|37|촌락|원목 24|점토 28|2|11|19166|
|38|촌락|점토 24|원목 28|2|11|19166|
|39|촌락|식량 36|약초 15|2|11|15000|
|40|촌락|원목 24|가죽 12|2|11|20000|
|41|촌락|가죽 9|식량 65|2|11|14074|
|42|촌락|약초 14|목재 13|2|11|14761|
|43|촌락|돌 16|벽돌 11|2|11|15208|
|44|마을|목재 12|벽돌 11|2|10|15208|
|45|마을|벽돌 10|목재 17|2|10|17200|
|46|마을|돌 20|철광석 16|2|10|14000|
|47|마을|철광석 10|돌 18|2|10|17000|
|48|마을|점토 22|벽돌 12|2|10|20000|
|49|마을|가죽 12|철광석 19|2|10|13750|
|50|마을|철광석 14|가죽 13|2|10|17857|
|51|마을|약초 18|벽돌 12|2|10|14444|
|52|마을|벽돌 11|약초 23|2|10|15090|
|53|마을|목재 14|철광석 15|2|10|14107|
|54|마을|식량 60|석탄 20|2|10|13333|
|55|마을|원목 30|철광석 19|2|10|18500|
|56|도시|철광석 16|석탄 23|2|9|15156|
|57|도시|석탄 20|철광석 18|2|9|17000|
|58|도시|철광석 12|철괴 6|2|9|17500|
|59|도시|석탄 16|철괴 6|2|9|17500|
|60|도시|목재 16|철괴 7|2|9|14062|
|61|도시|철괴 4|목재 13|2|9|11666|
|62|도시|가죽 15|철괴 10|1|9|14444|
|63|도시|돌 30|철광석 24|2|9|14000|
|64|도시|식량 80|벽돌 17|2|9|13375|
|65|도시|약초 20|석탄 21|2|9|14500|
|66|도시|벽돌 15|석탄 26|2|9|14266|
|67|도시|점토 30|철광석 20|2|9|20000|
|68|성채|금광석 3|금괴 2|1|8|12222|
|69|성채|금광석 4|철괴 7|2|8|11388|
|70|성채|철괴 6|금광석 5|2|8|10833|
|71|성채|금괴 1|철괴 4|2|8|12000|
|72|성채|철괴 7|금괴 3|1|8|11428|
|73|성채|석탄 25|금광석 7|2|8|18000|
|74|성채|금광석 3|벽돌 10|2|8|10370|
|75|성채|벽돌 20|철괴 12|1|8|16400|
|76|성채|가죽 20|금광석 9|2|8|12500|
|77|성채|목재 23|금광석 7|2|8|12826|
|78|왕성|금괴 3|철괴 10|2|7|8333|
|79|왕성|금괴 2|금광석 5|2|7|8750|
|80|왕성|금광석 8|금괴 5|1|7|10833|
|81|왕성|철괴 10|금광석 9|2|7|12500|
|82|왕성|금괴 2|약초 25|2|7|8750|
|83|왕성|석탄 28|금괴 4|1|7|18571|
|84|왕성|가죽 25|금괴 6|1|7|14000|
|85|왕성|벽돌 18|금괴 4|1|7|16666|
