# Аутентификация

После успешной установки сетевого соединения сервер принимает к обработке запросы от клиента, в которых передается аутентификационная информация. На ее основании сервер определяет учетную запись (аккаунт) клиента и проверяет доступ на выполнение запроса.

Поддерживаются следующие режимы аутентификации:

* Анонимный доступ — включен по умолчанию и доступен сразу после [установки кластера](../deploy/index.md).
* Аутентификация с использованием стороннего IAM-провайдера, например [Yandex Identity and Access Management]{% if lang == "en" %}(https://cloud.yandex.com/en/docs/iam/){% endif %}{% if lang == "ru" %}(https://cloud.yandex.ru/docs/iam/){% endif %}.
* Аутентификация по [логину и паролю](#static-credentions).

{% include [overlay/auth_choose.md](_includes/connect_overlay/auth_choose.md) %}

## Аутентификация по логину и паролю {#static-credentions}

Процесс аутентификации по логину и паролю включает следующие шаги:

1. Клиент обращается к БД и передает логин и пароль пользователя сервису аутентификации {{ ydb-short-name }}.

    Имя пользователя может содержать только строчные буквы латинского алфавита и цифры. Требования к паролю не предъявляются, пароль может быть пустым.
1. Сервис аутентификации передает аутентификационные данные в компонент аутентификации {{ ydb-short-name }}.
1. Компонент валидирует аутентификационные данные, при успешном сопоставлении создает токен и возвращает его сервису аутентификации.

    Использование токена ускоряет процесс аутентификации и повышает безопасность. Время жизни токена по умолчанию 12 часов. Для ротации токенов YDB SDK самостоятельно обращается к сервису аутентификации.

    Имя пользователя и хеш пароля хранятся в таблице внутри компонента аутентификации. Пароль хеширован методом [Argon2]{% if lang == "en" %}(https://en.wikipedia.org/wiki/Argon2){% endif %}{% if lang == "ru" %}(https://ru.wikipedia.org/wiki/Argon2){% endif %}. В режиме аутентификации по логину и паролю доступ к таблице имеет только администратор системы.
1. Сервис аутентификации возвращает токен клиенту.
1. Клиент обращается к БД, передавая в качестве аутентификационной информации токен.

Для включения аутентификации по логину и паролю укажите значение `true` для ключа `enforce_user_token_requirement` в [конфигурационном файле](../deploy/configuration/config.md#auth) кластера.

Об управлении ролями и пользователями читайте в [{#T}](../cluster/access.md).

<!-- ### API получения токенов IAM {#token-refresh-api}

Для ротации токенов {{ ydb-short-name }} SDK и CLI используют gRPC-запрос к {{ yandex-cloud }} IAM API [IamToken - create]{% if lang == "en" %}(https://cloud.yandex.com/en/docs/iam/api-ref/grpc/iam_token_service#Create){% endif %}{% if lang == "ru" %}(https://cloud.yandex.ru/docs/iam/api-ref/grpc/iam_token_service#Create){% endif %}. В режиме **Refresh Token** заданный параметром OAuth токен передается в атрибуте `yandex_passport_oauth_token`. В режиме **Service Account Key** на основании заданных атрибутов сервисной учетной записи и ключа шифрования формируется короткоживущий JWT-токен и передается в атрибуте `jwt`. Исходный код формирования JWT-токена доступен в составе SDK (например, метод `get_jwt()` в [коде на Python](https://github.com/ydb-platform/ydb-python-sdk/blob/main/ydb/iam/auth.py)).

{{ ydb-short-name }} SDK и CLI позволяют подменить хост для обращения к API получения токенов, что дает возможность реализовать аналогичный API в корпоративных контекстах. -->