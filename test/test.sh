cd ../build

cmake ..

make -j$(nproc)

cd ../test

python run.py S

python score.py S
