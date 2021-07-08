$frame frame1 frame2

float time;
entity self;
.float frame;
.float nextthink;
.void() think;

// Mixing syntax, = is optional.
void frame1_func_mixed_no_assign() [$frame1, frame2_func_mixed_no_assign] {}
void frame2_func_mixed_no_assign() [$frame2, frame2_func_mixed_no_assign] {}

void frame1_func_mixed() =[$frame1, frame2_func_mixed] {}
void frame2_func_mixed() =[$frame2, frame2_func_mixed] {}

void() frame1_func_old =[$frame1, frame2_func_old] {}
void() frame2_func_old =[$frame2, frame2_func_old] {}



