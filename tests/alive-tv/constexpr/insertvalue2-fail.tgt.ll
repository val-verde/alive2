define {{i8, i8}, {i8, i8}} @func() {
  ; {{5, 6}, {8, 10}}
  ret { {i8 ,i8}, {i8, i8}} { {i8, i8} {i8 5, i8 6}, {i8, i8} {i8 8, i8 10}}
}