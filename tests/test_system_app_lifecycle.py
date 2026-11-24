from pathlib import Path


REPOSITORY_ROOT = Path(__file__).parents[1]


def test_system_app_is_not_stopped_with_regular_app_api():
    for relative_path in (
        "server/libsoup/2.4/gdial-rest.c",
        "server/libsoup/3.0/gdial-rest.c",
    ):
        source = (REPOSITORY_ROOT / relative_path).read_text()
        assert 'if (strcmp(app->name, "system") == 0)' in source
        assert "gdial_app_force_shutdown(app);" in source
        assert "} else if (gdial_app_stop(app) == GDIAL_APP_ERROR_NONE)" in source