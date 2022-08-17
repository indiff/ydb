Все спорные вопросы, пожелания по процессу, фичереквесты можно присылать на [arcadia-wg@](mailto:arcadia-wg@yandex-team.ru)

## Что такое contrib?

Это набор общих компонент в Аркадии из внешнего интернета, от которых зависят проекты внутри Аркадии.

Компоненты в contrib подразделяются на ЯП-специфичные, общие тулы (англ. tools) и библиотеки:

* [contrib/java](https://a.yandex-team.ru/arc/trunk/arcadia/contrib/java)
* [contrib/python](https://a.yandex-team.ru/arc/trunk/arcadia/contrib/python)
* [vendor (aka go)](https://a.yandex-team.ru/arc/trunk/arcadia/vendor)
* [contrib/libs (aka C++)](https://a.yandex-team.ru/arc/trunk/arcadia/contrib/libs)
* [contrib/tools (общие тулы (jdk, python, cython)](https://a.yandex-team.ru/arc/trunk/arcadia/contrib/tools)

Каждый компонент внутри contrib собирается ya make и добавляется при соблюдении нескольких условий, в том числе подтверждении от профильных групп людей.

## Добавление библиотеки в contrib

Перед добавлением библиотеки нужно убедиться, что библиотека действительно полезна. См. ниже про то, "Как комитеты согласуют новые библиотеки в contrib". Это те факторы, которые стоит учесть до начала процесса согласования. Возможно, в процессе анализа вы найдете более подходящие альтернативы в Аркадии или внешнем мире.

Добавление новой библиотеки начинается с **[создания тикета в очереди CONTRIB](https://st.yandex-team.ru/createTicket?queue=CONTRIB)**.

## Мне нужна библиотека, написанная в Яндексе, которой нет в Аркадии. Можно ли положить её в contrib?

Нет, `contrib/` — место для внешнего кода, которым мы (компания Яндекс) не владеем.

В случае внутренней разработки, алгоритм следующий:
1. У библиотеки есть команда разработки. В таком случае (при наличии пользователей в Аркадии) разработка библиотеки должна переехать в Аркадию. 
   Если это невозможно, команда должна обосновать, почему такой переезд невозможен, и наладить регулярную синхронизацию в Аркадию кода библиотеки
   (также можно выполнять синхронизацию кода в Аркадию на период переезда). Владельцами кода библиотеки в Аркадии становятся разработчики библиотеки.
2. У библиотеки нет команды разработки. В таком случае автор становится мейнтейнером, либо находит мейнтейнера среди прочих пользователей библиотеки. 
   Библиотеку следует положить в место для общеупотребимых библиотек (`library/`, `library/python/`) или встроить в проект-пользователь в зависимости от того, 
   представляет ли она интерес широкому кругу разработчиков.

Из этого правила есть де-факто исключения для библиотек, которые были помещены в `contrib/` до его введения. Впоследствии они должны быть исключены из contrib.
Список таких библиотек приведён в тикете: [CONTRIB-396](https://st.yandex-team.ru/CONTRIB-396)

## Обновление библиотеки в Contrib

Если в обновлении библиотеки нужна помощь Devtools и/или это мажорное обновление библиотеки, то это лучше проводить через тикет в очереди CONTRIB.

Минорные обновления можно сделать самостоятельно до зеленой проверки, для этого по умолчанию никакие подтверждения и тикеты в очереди CONTRIB не требуются.
Разделение на минорные и мажорное обновление следует понимать "по духу", а не "по номеру версии". Например, обновление openssl с 1.0 на 1.1 – это задача на пол года,
несмотря на то что изменилась минорная версия.

А если вы обновляете библиотеку, которую используют всего пара проектов, то можно со всеми потребителями договориться сразу в PR.
Тикет не обязателен.

## Команда, правила и процессы Contrib

Текущий процесс относится к каждому ЯП в Аркадии: C++, Java, Python, Go
Весь текущий и грядущий workflow работы команд, отвечающих за общие библиотеки, реализован на базе Стартрека и очереди **[Contrib](https://st.yandex-team.ru/contrib)**.

Happy Path для пользователя, которому понадобилась новая библиотека в Аркадии, выглядит следующим образом
![](https://jing.yandex-team.ru/files/alexeykruglov/contrib1.png)

Основная задача _**Профильного комитета**_ – помочь пользователю, сообщив о наличии уже существующих решений на релевантном или другом ЯП в Аркадии (в util/library/contrib). В профильном комитете необходимо 2 человека, чтобы сделать окончательное решение.

[Arcadia WG](https://abc.yandex-team.ru/services/arcadia-wg/) привлекается профильными комитетами для обсуждения сложных случаев (есть сомнения, проблемы с лицензиями).

Ограничения использования библиотек контролируется [макросами управления лицензиями](https://docs.yandex-team.ru/ya-make/general/rules#licenzii).

Итого, процесс состоит из этапов:

* Подтверждение от языкового комитета (1)
* Подтверждение от языкового комитета (2)
* Подтверждение от Arcadia WG, при необходимости. Все библиотеки с [запрещенными](https://wiki.yandex-team.ru/devrules/overall/aboutlicences/#zapreshhennyelicenzii) лицензиями требуют ок от Arcadia WG.
* Импорт библиотеки
  * Самостоятельно
  * Или с помощью Devtools

Процесс импорта кода библиотеки (самостоятельно или с помощью Devtools) описан тут: [Как импортировать код в contrib](https://wiki.yandex-team.ru/devrules/overall/Contrib/how-to-import-contrib/)

Итоговый процесс импорта библиотеки выглядит следующим образом:
![alt](https://jing.yandex-team.ru/files/alexeykruglov/contrib2.png)

## Java (note)

1. Для java "обновление библиотеки" – это заливка новой версии, тикет в этом случае необязателен.
2. На время переезда, библиотеку, разрабатываемую в Яндексе можно положить в контриб, если нет возможность перевезти её в Аркадию. На это нужен тикет. После заезда библиотеки в Аркадию, её нужно будет удалить из contrib и перейти на зависимость по исходникам.

## SLA (committees)

Мы переложили некоторую часть вышеописанного процесса на рельсы Трекера с его тригерами и SLA.

**5 дней** на каждый этап с участием комитета (за исключением этапа сборки библиотеки силами Devtools). С течением времени мы ожидаем оптимизации этой границы в меньшую сторону.

Для примера, – на ответ от профильного комитета (i.e. Python) даётся 5 рабочих дней.

Уведомления о нарушении SLA приходят заранее (пока только кому:rudskoy и кому:saint), и мы постараемся своевременно на это реагировать.

## Для переезжающих в Аркадию проектов

Базовые рекомендации по процессу:
1. Не забывайте указывать родительский тикет для переезда из очереди ARCADIA и/или MIGRATION.
2. По умолчанию создавайте отдельные задачи на отдельные зависимости.

Мы допускаем, что зависимостей может быть по-настоящему много, поэтому вы можете перечислить зависимости большими пачками или одним большим списком при создании тикета через форму.

Но исполнитель задачи, при необходимости обсудить какую-либо библиотеку, попросит автора унести это обсуждение в отдельную задачу.

## Проектные contrib

В Аркадии не допускается существование проектных contrib. Это усложняет поиск по коду, его переиспользование и аудит лицензий.

В процессе миграции допускается временно смигрировать библиотеку, у которой есть явные аналоги в Аркадии. Это возможно лишь при наличии коммитментов от команды со сроком на переезд на общеаркадийные компоненты.

В некотором приближении мы будем запрещать для проектов внутри Аркадии зависеть от таких библиотек.

## Как комитеты согласуют новые библиотеки в contrib

**Зачем нужно согласовывать новые библиотеки.**
Чтобы сообщить пользователю о наличии уже существующих решений на релевантном или другом ЯП в Аркадии или более подходящих решений из внешнего мира.

Что принимают во внимание комитеты при принятии решения, можно ли разрешать добавлять библиотеку в Аркадию и с какими ограничениями.

**Название проекта и описание потребности**
Понимая потребность, комитеты в некоторых случаях могут предложить более подходящее решение.
Наличие кода в junk не может быть аргументом при разрешении контрибов в Аркадии. Если нужно только проверить какую-либо библиотеку, это можно сделать локально, без коммита.

**Лицензия**
Ознакомьтесь с текстом [про лицензии](https://wiki.yandex-team.ru/devrules/overall/aboutlicences/).
Если лицензия библиотеки входит в список [запрещенных](https://wiki.yandex-team.ru/devrules/overall/aboutlicences/#zapreshhennyelicenzii), необходимо обсудить возможность использования библиотеки с юристами.
Если лицензия библиотеки отсутствует в документе, необходимо обратиться в Arcadia WG.

**Альтернативные библиотеки**
Важно сравнить библиотеку с альтернативными библиотеками (в том числе с нашими внутренними наработками), решающими ту же задачу. Ожидается что автор заявки проделает работу по поиску альтернатив и сравнит новую библиотеку с альтернативами по различным критериям: возможности, популярность, поддержка (дата последнего релиза).
Если в Аркадии есть похожие библиотеки и новая библиотека не дает заметных преимуществ, это может служить основанием для отказа.
Наличие более популярных и развитых библиотек во внешнем мире также может служить основанием для отказа.

**Транзитивные зависимости**
Если библиотека тянет за собой много зависимостей, это может служить плохим сигналом.

**Дата последнего релиза**
Если библиотека активно не развивается, то чаще всего (но не всегда) это означает то, что библиотека заброшена.

**Собираемость под какие платформы необходима**
В некоторых случаях это может служить усложняющим фактором. Если библиотека изначально не умеет собираться под требуемую платформу.

**Автор кода**
Если библиотека написана в Яндексе, то её не нужно класть в contrib. См. подробнее [тут](https://a.yandex-team.ru/arc/trunk/arcadia/contrib/#мне-нужна-библиотека-написанная-в-яндексе-которой-нет-в-аркадии-можно-ли-положить-её-в-contrib).

На данном этапе при полных ответах на вопросы комитеты должны понять, стоит ли добавлять библиотеку в Аркадию и может ли она быть полезна другим проектам. Если библиотеку добавлять не хочется, то следующие уточняющие вопросы могут помочь принять положительное решение, но обычно с [ограничениями](https://wiki.yandex-team.ru/devrules/overall/peerdirprohibition/) к использованию в других проектах.

**Идет ли миграция в Аркадию**
В процессе миграции допускается временно смигрировать библиотеку, у которой есть явные аналоги в Аркадии. Это возможно лишь при наличии коммитментов от команды со сроком на переезд на общеаркадийные компоненты. Другие проекты при этом [не смогут](https://wiki.yandex-team.ru/devrules/overall/peerdirprohibition/) использовать данную библиотеку.

**Что случится, если будет отказ**
Сколько стоит переход на альтернативные решения и т.д.

## После импорта

### Подключение к автосборке

Добавленную библиотеку нужно подключить к автосборке.
По умолчанию считается, что все добавленные contrib-ы должны собираться под серверный Linux.
Подключить контриб к дополнительным платформам можно в [autocheck/$PLATFORM](https://arcanum.yandex-team.ru/arc/trunk/arcadia/autocheck).

### Тесты на contrib

Тесты контрибов делятся на нативные (существующие в апстриме) и написанные внутри (далее — кастомные).
Последние могут понадобиться, чтобы проверить, что контриб обеспечивает какую-то функциональность
(например, работу с кодировкой CP1251, про которую в апстриме никто не знает) и чтобы удостовериться,
что контриб правильно работает в условиях аркадийной сборки в монобинарь.

Нативные тесты можно подключить к автосборке, если они используют поддерживаемый в Аркадии фреймворк для тестирования 
(`BOOSTTEST` или `GTEST` для С++, `PY23_TEST` / `PY3TEST` для Python).

Кастомные тесты пишутся точно также, как и обычные аркадийные тесты (см. [документацию](https://docs.yandex-team.ru/arcadia-cpp/cpp_test)).
Такие тесты нужно сложить в директорию [devtools/contrib_tests](https://arcanum.yandex-team.ru/arc/trunk/arcadia/devtools/contrib_tests).

## Как работает контрибная автоматика
После заведения CONTRIB-тикета приходит автоматика и по языку из компонента выбирает тех, кто будет одобрять библиотеку. Список одобряющих прописан в скрипте.
Автоматика живёт [тут](https://a.yandex-team.ru/arc/trunk/arcadia/vcs/manage_contrib/find.py). Запускается из Sandbox, Scheduler [тут](https://sandbox.yandex-team.ru/scheduler/21260/view).