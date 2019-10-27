# HERORM2020
该代码主要为RoboMaster2020赛季HERO团队视觉组交流使用。

## 环境依赖
- ubuntu 16.04及以上
- opencv3.4及以上
- cmake3.5.1及以上

## 编译运行
```sh
git clone ${this project}
mv p_5c39354b-master HERORM2020
cd HERORM2020
mkdir build
cd build
cmake ..
make
./HERORM2020
```

## update
- 2019.10.21.
    - 上传经测试的弹道计算代码：
      主要算法是先将坐标旋转变换至ｙ轴竖直的坐标系，再进行二分法解算（速度最快、准确度高);
      附加未增加旋转变换的二分法解算和速度矢量三角推公式算法的代码供备用（效果与旋转变换过的无明显差异，解算速度较慢）
      
- 2019.10.23.
    - 按照编程规范修改代码格式，保证编译无错；
    - 由于通讯函数尚未写好，弹道计算函数在workspace中调用的是无坐标转换的算法
