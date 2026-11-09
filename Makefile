PYTHON ?= python3
.PHONY: core final-check performance release json2png2 map observer-check civic-check work-check all cli app smoke test check check-deps audit run clean capacity lifecycle identity memory content content-check runtime-content city-check human-check
all: cli
cli:
	$(PYTHON) tools/build.py cli
json2png2:
	$(PYTHON) tools/build_basemap.py --write
map: json2png2
app:
	$(PYTHON) tools/build.py app
smoke:
	$(PYTHON) tools/build.py smoke
check-deps:
	$(PYTHON) tools/build.py check
audit:
	$(PYTHON) tools/audit_sources.py --check
	$(PYTHON) tools/verify_cycle01.py --check
	$(PYTHON) tools/verify_cycle02.py --check
capacity:
	$(PYTHON) tools/verify_cycle04.py --run
lifecycle:
	$(PYTHON) tools/verify_cycle05.py --run
identity:
	$(PYTHON) tools/verify_cycle06.py --run
memory:
	$(PYTHON) tools/verify_cycle07.py --run
content:
	$(PYTHON) tools/compile_content.py --write
content-check:
	$(PYTHON) tools/compile_content.py --check
runtime-content:
	$(PYTHON) tools/compile_runtime.py --write
city-check:
	$(PYTHON) tools/verify_city_cycles.py --run
observer-check:
	$(PYTHON) tools/verify_observer_cycles.py --run
civic-check:
	$(PYTHON) tools/verify_civic_cycles.py --run
work-check:
	$(PYTHON) tools/verify_work_cycles.py --run
human-check:
	$(PYTHON) tools/verify_human_cycles.py --run
test check:
	$(PYTHON) tools/verify_cycle03.py --run
	$(PYTHON) tools/verify_cycle04.py --run
	$(PYTHON) tools/verify_cycle05.py --run
	$(PYTHON) tools/verify_cycle06.py --run
	$(PYTHON) tools/verify_cycle07.py --run
	$(PYTHON) tools/verify_cycle08.py --run
	$(PYTHON) tools/verify_city_cycles.py --run
	$(PYTHON) tools/verify_human_cycles.py --run
	$(PYTHON) tools/verify_work_cycles.py --run
	$(PYTHON) tools/verify_civic_cycles.py --run
	$(PYTHON) tools/verify_observer_cycles.py --run
run: cli
	./build/CanterburyDetails --minutes 5
clean:
	$(PYTHON) -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'

core:
	$(PYTHON) tools/build_core.py
final-check:
	$(PYTHON) tools/verify_final_cycles.py --run
performance:
	$(PYTHON) tools/verify_final_cycles.py --performance
release:
	$(PYTHON) tools/verify_release.py --prepare
